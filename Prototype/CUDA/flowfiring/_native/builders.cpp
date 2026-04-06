/*  Builder functions: lattice_3d, grid_2d, grid_2d_quad, grid_3d.
 *
 *  Each builder constructs edges, finds faces, builds boundary operator,
 *  computes Laplacian (B*B^T) via Eigen, extracts CSC arrays + degrees,
 *  and optionally colors the conflict graph.
 *
 *  Returns a py::dict with all arrays needed for firing and visualization.
 */

#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <cstdint>
#include <vector>
#include <array>
#include <cstring>
#include <algorithm>
#include <unordered_set>
#include <Eigen/Sparse>

namespace py = pybind11;

// Existing C++ functions
extern void build_lattice_3d(int size, double* v0, double* v1,
                             int32_t* types, int32_t* dirs);

extern void build_simplicial_complex(
    const double* edge_verts, int num_edges,
    std::vector<std::array<int,3>>& faces_out,
    std::vector<int>& bnd_row,
    std::vector<int>& bnd_col,
    std::vector<int>& bnd_val);

extern void color_conflict_graph_native(
    const int32_t* indptr,
    const int32_t* indices,
    int n,
    int32_t* colors_out);

// ---------------------------------------------------------------------------
// Laplacian computation: L = B * B^T in CSC via Eigen
// ---------------------------------------------------------------------------

struct LaplacianCSC {
    std::vector<int32_t> indptr;
    std::vector<int32_t> indices;
    std::vector<int32_t> data;
    std::vector<int32_t> degrees;
};

static LaplacianCSC compute_laplacian_csc(
    const int* bnd_row, const int* bnd_col, const int* bnd_val,
    int nnz, int num_edges, int num_faces)
{
    using SpMat = Eigen::SparseMatrix<int32_t, Eigen::ColMajor>;
    using Triplet = Eigen::Triplet<int32_t>;

    std::vector<Triplet> triplets;
    triplets.reserve(nnz);
    for (int k = 0; k < nnz; ++k)
        triplets.emplace_back(bnd_row[k], bnd_col[k], bnd_val[k]);

    SpMat B(num_edges, num_faces);
    B.setFromTriplets(triplets.begin(), triplets.end());

    SpMat L = B * B.transpose();
    L.makeCompressed();

    LaplacianCSC result;
    int n = L.cols();
    int lnnz = L.nonZeros();

    result.indptr.resize(n + 1);
    result.indices.resize(lnnz);
    result.data.resize(lnnz);
    result.degrees.resize(n, 0);

    std::memcpy(result.indptr.data(), L.outerIndexPtr(), (n + 1) * sizeof(int32_t));
    std::memcpy(result.indices.data(), L.innerIndexPtr(), lnnz * sizeof(int32_t));
    std::memcpy(result.data.data(), L.valuePtr(), lnnz * sizeof(int32_t));

    // Degrees = diagonal of L
    for (int j = 0; j < n; ++j) {
        for (int p = result.indptr[j]; p < result.indptr[j + 1]; ++p) {
            if (result.indices[p] == j) {
                result.degrees[j] = result.data[p];
                break;
            }
        }
    }

    return result;
}

// ---------------------------------------------------------------------------
// Coloring helper: returns (color_offsets, color_edges) flat arrays
// ---------------------------------------------------------------------------

struct ColoringResult {
    std::vector<int32_t> color_offsets;
    std::vector<int32_t> color_edges;
};

static ColoringResult compute_coloring(
    const int32_t* indptr, const int32_t* indices, int num_edges)
{
    std::vector<int32_t> colors(num_edges);
    color_conflict_graph_native(indptr, indices, num_edges, colors.data());

    int num_colors = 0;
    for (int i = 0; i < num_edges; ++i)
        if (colors[i] >= num_colors) num_colors = colors[i] + 1;

    // Count edges per color
    std::vector<int> counts(num_colors, 0);
    for (int i = 0; i < num_edges; ++i)
        ++counts[colors[i]];

    ColoringResult result;
    result.color_offsets.resize(num_colors + 1, 0);
    for (int c = 0; c < num_colors; ++c)
        result.color_offsets[c + 1] = result.color_offsets[c] + counts[c];

    result.color_edges.resize(num_edges);
    std::vector<int> fill(num_colors, 0);
    for (int i = 0; i < num_edges; ++i) {
        int c = colors[i];
        result.color_edges[result.color_offsets[c] + fill[c]++] = i;
    }

    return result;
}

// ---------------------------------------------------------------------------
// Helper: pack common keys into a py::dict
// ---------------------------------------------------------------------------

static void pack_laplacian(py::dict& d, const LaplacianCSC& lap, int num_edges, int num_faces) {
    auto indptr = py::array_t<int32_t>(lap.indptr.size());
    auto indices = py::array_t<int32_t>(lap.indices.size());
    auto data = py::array_t<int32_t>(lap.data.size());
    auto degrees = py::array_t<int32_t>(lap.degrees.size());

    std::memcpy(indptr.mutable_data(), lap.indptr.data(), lap.indptr.size() * sizeof(int32_t));
    std::memcpy(indices.mutable_data(), lap.indices.data(), lap.indices.size() * sizeof(int32_t));
    std::memcpy(data.mutable_data(), lap.data.data(), lap.data.size() * sizeof(int32_t));
    std::memcpy(degrees.mutable_data(), lap.degrees.data(), lap.degrees.size() * sizeof(int32_t));

    d["indptr"] = indptr;
    d["indices"] = indices;
    d["data"] = data;
    d["degrees"] = degrees;
    d["num_edges"] = num_edges;
    d["num_faces"] = num_faces;
}

static void pack_boundary(py::dict& d,
                          const std::vector<int>& bnd_row,
                          const std::vector<int>& bnd_col,
                          const std::vector<int>& bnd_val) {
    int nnz = (int)bnd_row.size();
    auto br = py::array_t<int32_t>(nnz);
    auto bc = py::array_t<int32_t>(nnz);
    auto bv = py::array_t<int32_t>(nnz);
    std::memcpy(br.mutable_data(), bnd_row.data(), nnz * sizeof(int32_t));
    std::memcpy(bc.mutable_data(), bnd_col.data(), nnz * sizeof(int32_t));
    std::memcpy(bv.mutable_data(), bnd_val.data(), nnz * sizeof(int32_t));
    d["bnd_row"] = br;
    d["bnd_col"] = bc;
    d["bnd_val"] = bv;
}

static void pack_coloring(py::dict& d, const LaplacianCSC& lap, int num_edges) {
    auto cr = compute_coloring(lap.indptr.data(), lap.indices.data(), num_edges);
    auto offsets = py::array_t<int32_t>(cr.color_offsets.size());
    auto edges = py::array_t<int32_t>(cr.color_edges.size());
    std::memcpy(offsets.mutable_data(), cr.color_offsets.data(), cr.color_offsets.size() * sizeof(int32_t));
    std::memcpy(edges.mutable_data(), cr.color_edges.data(), cr.color_edges.size() * sizeof(int32_t));
    d["color_offsets"] = offsets;
    d["color_edges"] = edges;
}

// ---------------------------------------------------------------------------
// Builder: lattice_3d
// ---------------------------------------------------------------------------

py::dict build_lattice_3d_full(int size, bool with_colors) {
    int total = 6 * size * size * size;

    // Step 1: build edge geometry
    std::vector<double> v0(total * 3), v1(total * 3);
    std::vector<int32_t> types(total), dirs(total);
    build_lattice_3d(size, v0.data(), v1.data(), types.data(), dirs.data());

    // Step 2: interleave into edge_verts (total, 2, 3)
    auto edge_verts = py::array_t<double>({total, 2, 3});
    double* ev_ptr = edge_verts.mutable_data();
    for (int i = 0; i < total; ++i) {
        ev_ptr[i * 6 + 0] = v0[i * 3 + 0];
        ev_ptr[i * 6 + 1] = v0[i * 3 + 1];
        ev_ptr[i * 6 + 2] = v0[i * 3 + 2];
        ev_ptr[i * 6 + 3] = v1[i * 3 + 0];
        ev_ptr[i * 6 + 4] = v1[i * 3 + 1];
        ev_ptr[i * 6 + 5] = v1[i * 3 + 2];
    }

    // Step 3: find faces + boundary
    std::vector<std::array<int,3>> faces;
    std::vector<int> bnd_row, bnd_col, bnd_val;
    build_simplicial_complex(ev_ptr, total, faces, bnd_row, bnd_col, bnd_val);

    int num_faces = (int)faces.size();

    // Step 4: Laplacian
    auto lap = compute_laplacian_csc(
        bnd_row.data(), bnd_col.data(), bnd_val.data(),
        (int)bnd_row.size(), total, num_faces);

    // Step 5: pack result
    py::dict d;
    d["edge_verts"] = edge_verts;
    pack_laplacian(d, lap, total, num_faces);
    pack_boundary(d, bnd_row, bnd_col, bnd_val);

    // Faces array (num_faces, 3)
    auto faces_arr = py::array_t<int32_t>({num_faces, 3});
    auto fptr = faces_arr.mutable_data();
    for (int i = 0; i < num_faces; ++i) {
        fptr[i * 3]     = faces[i][0];
        fptr[i * 3 + 1] = faces[i][1];
        fptr[i * 3 + 2] = faces[i][2];
    }
    d["faces"] = faces_arr;

    // Edge metadata
    auto et = py::array_t<int32_t>(total);
    auto ed = py::array_t<int32_t>(total);
    std::memcpy(et.mutable_data(), types.data(), total * sizeof(int32_t));
    std::memcpy(ed.mutable_data(), dirs.data(), total * sizeof(int32_t));
    d["edge_types"] = et;
    d["edge_dirs"] = ed;

    if (with_colors)
        pack_coloring(d, lap, total);

    return d;
}

// ---------------------------------------------------------------------------
// Builder: grid_2d (triangulated)
// ---------------------------------------------------------------------------

py::dict build_grid_2d_full(int rows, int cols,
                            bool has_hollow, int x_lo, int y_lo, int x_hi, int y_hi,
                            bool with_colors) {
    // Determine hollow cells
    auto is_hollow_cell = [&](int i, int j) {
        return has_hollow && i >= x_lo && i < x_hi && j >= y_lo && j < y_hi;
    };

    // Build edges
    std::vector<double> ev_flat;  // (v0x, v0y, v0z, v1x, v1y, v1z) per edge
    std::vector<int32_t> edge_types_vec;

    // Horizontal edges: (i,j,0) -> (i+1,j,0) for j in [0,rows], i in [0,cols)
    for (int j = 0; j <= rows; ++j) {
        for (int i = 0; i < cols; ++i) {
            if (has_hollow && i >= x_lo && i < x_hi && j > y_lo && j < y_hi)
                continue;
            ev_flat.insert(ev_flat.end(), {(double)i, (double)j, 0.0,
                                           (double)(i+1), (double)j, 0.0});
            edge_types_vec.push_back(0);  // h
        }
    }

    // Vertical edges: (i,j,0) -> (i,j+1,0) for i in [0,cols], j in [0,rows)
    for (int i = 0; i <= cols; ++i) {
        for (int j = 0; j < rows; ++j) {
            if (has_hollow && i > x_lo && i < x_hi && j >= y_lo && j < y_hi)
                continue;
            ev_flat.insert(ev_flat.end(), {(double)i, (double)j, 0.0,
                                           (double)i, (double)(j+1), 0.0});
            edge_types_vec.push_back(1);  // v
        }
    }

    // Diagonal edges: (i,j,0) -> (i+1,j+1,0) for non-hollow cells
    for (int i = 0; i < cols; ++i) {
        for (int j = 0; j < rows; ++j) {
            if (is_hollow_cell(i, j)) continue;
            ev_flat.insert(ev_flat.end(), {(double)i, (double)j, 0.0,
                                           (double)(i+1), (double)(j+1), 0.0});
            edge_types_vec.push_back(2);  // d
        }
    }

    int num_edges = (int)edge_types_vec.size();

    // Find faces + boundary via simplicial complex builder
    std::vector<std::array<int,3>> faces;
    std::vector<int> bnd_row, bnd_col, bnd_val;
    build_simplicial_complex(ev_flat.data(), num_edges, faces, bnd_row, bnd_col, bnd_val);

    int num_faces = (int)faces.size();

    // Laplacian
    auto lap = compute_laplacian_csc(
        bnd_row.data(), bnd_col.data(), bnd_val.data(),
        (int)bnd_row.size(), num_edges, num_faces);

    // Pack result
    py::dict d;

    auto edge_verts = py::array_t<double>({num_edges, 2, 3});
    std::memcpy(edge_verts.mutable_data(), ev_flat.data(), num_edges * 6 * sizeof(double));
    d["edge_verts"] = edge_verts;

    pack_laplacian(d, lap, num_edges, num_faces);
    pack_boundary(d, bnd_row, bnd_col, bnd_val);

    auto faces_arr = py::array_t<int32_t>({num_faces, 3});
    auto fptr = faces_arr.mutable_data();
    for (int i = 0; i < num_faces; ++i) {
        fptr[i * 3]     = faces[i][0];
        fptr[i * 3 + 1] = faces[i][1];
        fptr[i * 3 + 2] = faces[i][2];
    }
    d["faces"] = faces_arr;

    auto et = py::array_t<int32_t>(num_edges);
    std::memcpy(et.mutable_data(), edge_types_vec.data(), num_edges * sizeof(int32_t));
    d["edge_types"] = et;

    if (with_colors)
        pack_coloring(d, lap, num_edges);

    return d;
}

// ---------------------------------------------------------------------------
// Builder: grid_2d_quad
// ---------------------------------------------------------------------------

py::dict build_grid_2d_quad_full(int rows, int cols,
                                 bool has_hollow, int x_lo, int y_lo, int x_hi, int y_hi,
                                 bool with_colors) {
    auto is_hollow_cell = [&](int i, int j) {
        return has_hollow && i >= x_lo && i < x_hi && j >= y_lo && j < y_hi;
    };

    // Build edges + track edge indices by key
    struct EdgeEntry { double v0[3]; double v1[3]; int32_t type; };
    std::vector<EdgeEntry> edges;

    // edge_index[("h"|"v", i, j)] -> edge index
    // Encode as: key = type * 1000000 + i * 1000 + j (sufficient for grids < 1000)
    // Actually, use a map for safety
    std::unordered_map<int64_t, int> edge_index;
    auto make_key = [](int type, int i, int j) -> int64_t {
        return (int64_t)type * 100000000LL + (int64_t)i * 10000LL + (int64_t)j;
    };

    // Horizontal edges
    for (int j = 0; j <= rows; ++j) {
        for (int i = 0; i < cols; ++i) {
            if (has_hollow && i >= x_lo && i < x_hi && j > y_lo && j < y_hi)
                continue;
            int idx = (int)edges.size();
            edge_index[make_key(0, i, j)] = idx;
            edges.push_back({{(double)i, (double)j, 0.0},
                              {(double)(i+1), (double)j, 0.0}, 0});
        }
    }

    // Vertical edges
    for (int i = 0; i <= cols; ++i) {
        for (int j = 0; j < rows; ++j) {
            if (has_hollow && i > x_lo && i < x_hi && j >= y_lo && j < y_hi)
                continue;
            int idx = (int)edges.size();
            edge_index[make_key(1, i, j)] = idx;
            edges.push_back({{(double)i, (double)j, 0.0},
                              {(double)i, (double)(j+1), 0.0}, 1});
        }
    }

    int num_edges = (int)edges.size();

    // Build faces and boundary directly (quad faces, no face-finding needed)
    std::vector<std::array<int,4>> faces;
    std::vector<int> bnd_row, bnd_col, bnd_val;

    for (int i = 0; i < cols; ++i) {
        for (int j = 0; j < rows; ++j) {
            if (is_hollow_cell(i, j)) continue;
            auto it_b = edge_index.find(make_key(0, i, j));
            auto it_r = edge_index.find(make_key(1, i + 1, j));
            auto it_t = edge_index.find(make_key(0, i, j + 1));
            auto it_l = edge_index.find(make_key(1, i, j));
            if (it_b == edge_index.end() || it_r == edge_index.end() ||
                it_t == edge_index.end() || it_l == edge_index.end())
                continue;

            int fi = (int)faces.size();
            int bottom = it_b->second, right = it_r->second;
            int top = it_t->second, left = it_l->second;
            faces.push_back({bottom, right, top, left});

            // Boundary: bottom(+1), right(+1), top(-1), left(-1)
            bnd_row.insert(bnd_row.end(), {bottom, right, top, left});
            bnd_col.insert(bnd_col.end(), {fi, fi, fi, fi});
            bnd_val.insert(bnd_val.end(), {1, 1, -1, -1});
        }
    }

    int num_faces = (int)faces.size();

    // Laplacian
    auto lap = compute_laplacian_csc(
        bnd_row.data(), bnd_col.data(), bnd_val.data(),
        (int)bnd_row.size(), num_edges, num_faces);

    // Pack result
    py::dict d;

    auto edge_verts = py::array_t<double>({num_edges, 2, 3});
    auto ev_ptr = edge_verts.mutable_data();
    for (int i = 0; i < num_edges; ++i) {
        ev_ptr[i * 6 + 0] = edges[i].v0[0];
        ev_ptr[i * 6 + 1] = edges[i].v0[1];
        ev_ptr[i * 6 + 2] = edges[i].v0[2];
        ev_ptr[i * 6 + 3] = edges[i].v1[0];
        ev_ptr[i * 6 + 4] = edges[i].v1[1];
        ev_ptr[i * 6 + 5] = edges[i].v1[2];
    }
    d["edge_verts"] = edge_verts;

    pack_laplacian(d, lap, num_edges, num_faces);
    pack_boundary(d, bnd_row, bnd_col, bnd_val);

    // Faces as flat array (num_faces * 4) — quad faces
    auto faces_arr = py::array_t<int32_t>({num_faces, 4});
    auto fptr = faces_arr.mutable_data();
    for (int i = 0; i < num_faces; ++i) {
        fptr[i * 4]     = faces[i][0];
        fptr[i * 4 + 1] = faces[i][1];
        fptr[i * 4 + 2] = faces[i][2];
        fptr[i * 4 + 3] = faces[i][3];
    }
    d["faces"] = faces_arr;

    auto et = py::array_t<int32_t>(num_edges);
    auto et_ptr = et.mutable_data();
    for (int i = 0; i < num_edges; ++i)
        et_ptr[i] = edges[i].type;
    d["edge_types"] = et;

    if (with_colors)
        pack_coloring(d, lap, num_edges);

    return d;
}

// ---------------------------------------------------------------------------
// Builder: grid_3d (simple cubic, no hollow)
// ---------------------------------------------------------------------------

py::dict build_grid_3d_full(int nx, int ny, int nz, bool with_colors,
                            py::array_t<int32_t> hollow_face_planes,
                            py::array_t<int32_t> hollow_face_coords) {
    // hollow_face_planes: int32 array of plane types (0=xy, 1=xz, 2=yz)
    // hollow_face_coords: int32 array of (i,j,k) triples, length = 3 * num_hollow
    // Build a set of hollow faces for O(1) lookup
    auto hfp = hollow_face_planes.unchecked<1>();
    auto hfc = hollow_face_coords.unchecked<1>();
    int num_hollow = (int)hfp.shape(0);

    struct HollowKey {
        int plane, i, j, k;
        bool operator==(const HollowKey& o) const {
            return plane == o.plane && i == o.i && j == o.j && k == o.k;
        }
    };
    struct HollowHash {
        size_t operator()(const HollowKey& h) const {
            size_t v = std::hash<int>{}(h.plane);
            v ^= std::hash<int>{}(h.i) + 0x9e3779b97f4a7c15ULL + (v << 6) + (v >> 2);
            v ^= std::hash<int>{}(h.j) + 0x9e3779b97f4a7c15ULL + (v << 6) + (v >> 2);
            v ^= std::hash<int>{}(h.k) + 0x9e3779b97f4a7c15ULL + (v << 6) + (v >> 2);
            return v;
        }
    };
    std::unordered_set<HollowKey, HollowHash> hollow_set;
    for (int h = 0; h < num_hollow; ++h)
        hollow_set.insert({hfp(h), hfc(h * 3), hfc(h * 3 + 1), hfc(h * 3 + 2)});

    // Build edges
    struct EdgeEntry { double v0[3]; double v1[3]; int32_t axis; int32_t ix, iy, iz; };
    std::vector<EdgeEntry> edges;

    // For face construction, track edge indices by (axis, i, j, k)
    // axis 0: x-edge at (i,j,k), axis 1: y-edge, axis 2: z-edge
    std::unordered_map<int64_t, int> edge_idx;
    auto make_key = [](int axis, int i, int j, int k) -> int64_t {
        return (int64_t)axis * 1000000000LL +
               (int64_t)i * 1000000LL +
               (int64_t)j * 1000LL +
               (int64_t)k;
    };

    // X-edges: (i,j,k) -> (i+1,j,k)
    for (int k = 0; k <= nz; ++k) {
        for (int j = 0; j <= ny; ++j) {
            for (int i = 0; i < nx; ++i) {
                int idx = (int)edges.size();
                edge_idx[make_key(0, i, j, k)] = idx;
                edges.push_back({{(double)i, (double)j, (double)k},
                                  {(double)(i+1), (double)j, (double)k},
                                  0, (int32_t)i, (int32_t)j, (int32_t)k});
            }
        }
    }

    // Y-edges: (i,j,k) -> (i,j+1,k)
    for (int k = 0; k <= nz; ++k) {
        for (int j = 0; j < ny; ++j) {
            for (int i = 0; i <= nx; ++i) {
                int idx = (int)edges.size();
                edge_idx[make_key(1, i, j, k)] = idx;
                edges.push_back({{(double)i, (double)j, (double)k},
                                  {(double)i, (double)(j+1), (double)k},
                                  1, (int32_t)i, (int32_t)j, (int32_t)k});
            }
        }
    }

    // Z-edges: (i,j,k) -> (i,j,k+1)
    for (int k = 0; k < nz; ++k) {
        for (int j = 0; j <= ny; ++j) {
            for (int i = 0; i <= nx; ++i) {
                int idx = (int)edges.size();
                edge_idx[make_key(2, i, j, k)] = idx;
                edges.push_back({{(double)i, (double)j, (double)k},
                                  {(double)i, (double)j, (double)(k+1)},
                                  2, (int32_t)i, (int32_t)j, (int32_t)k});
            }
        }
    }

    int num_edges = (int)edges.size();

    // Build faces and boundary (quad faces)
    std::vector<std::array<int,4>> faces;
    std::vector<int> bnd_row, bnd_col, bnd_val;

    // Removed faces: edges and signs for each removed face
    std::vector<std::array<int,4>> removed_edges;
    std::vector<std::array<int,4>> removed_signs;

    auto add_face = [&](int plane, int fi_i, int fi_j, int fi_k,
                        int e_bottom, int e_right, int e_top, int e_left) {
        if (hollow_set.count({plane, fi_i, fi_j, fi_k})) {
            removed_edges.push_back({e_bottom, e_right, e_top, e_left});
            removed_signs.push_back({1, 1, -1, -1});
            return;
        }
        int fi = (int)faces.size();
        faces.push_back({e_bottom, e_right, e_top, e_left});
        bnd_row.insert(bnd_row.end(), {e_bottom, e_right, e_top, e_left});
        bnd_col.insert(bnd_col.end(), {fi, fi, fi, fi});
        bnd_val.insert(bnd_val.end(), {1, 1, -1, -1});
    };

    // XY-faces (plane=0): at (i,j,k) for 0<=i<nx, 0<=j<ny, 0<=k<=nz
    for (int k = 0; k <= nz; ++k) {
        for (int j = 0; j < ny; ++j) {
            for (int i = 0; i < nx; ++i) {
                add_face(0, i, j, k,
                    edge_idx[make_key(0, i, j, k)],
                    edge_idx[make_key(1, i+1, j, k)],
                    edge_idx[make_key(0, i, j+1, k)],
                    edge_idx[make_key(1, i, j, k)]
                );
            }
        }
    }

    // XZ-faces (plane=1): at (i,j,k) for 0<=i<nx, 0<=j<=ny, 0<=k<nz
    for (int k = 0; k < nz; ++k) {
        for (int j = 0; j <= ny; ++j) {
            for (int i = 0; i < nx; ++i) {
                add_face(1, i, j, k,
                    edge_idx[make_key(0, i, j, k)],
                    edge_idx[make_key(2, i+1, j, k)],
                    edge_idx[make_key(0, i, j, k+1)],
                    edge_idx[make_key(2, i, j, k)]
                );
            }
        }
    }

    // YZ-faces (plane=2): at (i,j,k) for 0<=i<=nx, 0<=j<ny, 0<=k<nz
    for (int k = 0; k < nz; ++k) {
        for (int j = 0; j < ny; ++j) {
            for (int i = 0; i <= nx; ++i) {
                add_face(2, i, j, k,
                    edge_idx[make_key(1, i, j, k)],
                    edge_idx[make_key(2, i, j+1, k)],
                    edge_idx[make_key(1, i, j, k+1)],
                    edge_idx[make_key(2, i, j, k)]
                );
            }
        }
    }

    int num_faces = (int)faces.size();

    // Laplacian
    auto lap = compute_laplacian_csc(
        bnd_row.data(), bnd_col.data(), bnd_val.data(),
        (int)bnd_row.size(), num_edges, num_faces);

    // Pack result
    py::dict d;

    auto edge_verts = py::array_t<double>({num_edges, 2, 3});
    auto ev_ptr = edge_verts.mutable_data();
    for (int i = 0; i < num_edges; ++i) {
        ev_ptr[i * 6 + 0] = edges[i].v0[0];
        ev_ptr[i * 6 + 1] = edges[i].v0[1];
        ev_ptr[i * 6 + 2] = edges[i].v0[2];
        ev_ptr[i * 6 + 3] = edges[i].v1[0];
        ev_ptr[i * 6 + 4] = edges[i].v1[1];
        ev_ptr[i * 6 + 5] = edges[i].v1[2];
    }
    d["edge_verts"] = edge_verts;

    pack_laplacian(d, lap, num_edges, num_faces);
    pack_boundary(d, bnd_row, bnd_col, bnd_val);

    // Faces (num_faces, 4)
    auto faces_arr = py::array_t<int32_t>({num_faces, 4});
    auto fptr = faces_arr.mutable_data();
    for (int i = 0; i < num_faces; ++i) {
        fptr[i * 4]     = faces[i][0];
        fptr[i * 4 + 1] = faces[i][1];
        fptr[i * 4 + 2] = faces[i][2];
        fptr[i * 4 + 3] = faces[i][3];
    }
    d["faces"] = faces_arr;

    // Edge metadata arrays
    auto ea = py::array_t<int32_t>(num_edges);
    auto eix = py::array_t<int32_t>(num_edges);
    auto eiy = py::array_t<int32_t>(num_edges);
    auto eiz = py::array_t<int32_t>(num_edges);
    auto ea_p = ea.mutable_data();
    auto eix_p = eix.mutable_data();
    auto eiy_p = eiy.mutable_data();
    auto eiz_p = eiz.mutable_data();
    for (int i = 0; i < num_edges; ++i) {
        ea_p[i] = edges[i].axis;
        eix_p[i] = edges[i].ix;
        eiy_p[i] = edges[i].iy;
        eiz_p[i] = edges[i].iz;
    }
    d["edge_axes"] = ea;
    d["edge_ix"] = eix;
    d["edge_iy"] = eiy;
    d["edge_iz"] = eiz;

    // Removed faces
    int num_removed = (int)removed_edges.size();
    auto rf_edges = py::array_t<int32_t>({num_removed, 4});
    auto rf_signs = py::array_t<int32_t>({num_removed, 4});
    auto rfe_p = rf_edges.mutable_data();
    auto rfs_p = rf_signs.mutable_data();
    for (int i = 0; i < num_removed; ++i) {
        for (int j = 0; j < 4; ++j) {
            rfe_p[i * 4 + j] = removed_edges[i][j];
            rfs_p[i * 4 + j] = removed_signs[i][j];
        }
    }
    d["removed_face_edges"] = rf_edges;
    d["removed_face_signs"] = rf_signs;

    if (with_colors)
        pack_coloring(d, lap, num_edges);

    return d;
}
