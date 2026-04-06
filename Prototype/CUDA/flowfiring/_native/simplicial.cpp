/*  Face-finding and boundary construction for SimplicialComplex.
 *
 *  Input:  edge_verts — flat double[num_edges * 6], row-major (N, 2, 3),
 *          already rounded to 4 decimal places.
 *  Output: faces (sorted triples of edge indices),
 *          boundary COO triplets (row=edge, col=face, val=±1).
 */

#include <cstdint>
#include <cmath>
#include <vector>
#include <array>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>


// --- Hash helpers ---

struct VertKey {
    int64_t x, y, z;
    bool operator==(const VertKey& o) const {
        return x == o.x && y == o.y && z == o.z;
    }
};

struct VertKeyHash {
    size_t operator()(const VertKey& k) const {
        size_t h = std::hash<int64_t>{}(k.x);
        h ^= std::hash<int64_t>{}(k.y) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
        h ^= std::hash<int64_t>{}(k.z) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
        return h;
    }
};

struct EdgeKey {
    int v0, v1;  // ordered: v0 <= v1
    bool operator==(const EdgeKey& o) const {
        return v0 == o.v0 && v1 == o.v1;
    }
};

struct EdgeKeyHash {
    size_t operator()(const EdgeKey& k) const {
        size_t h = std::hash<int>{}(k.v0);
        h ^= std::hash<int>{}(k.v1) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
        return h;
    }
};

struct FaceKey {
    int e0, e1, e2;  // sorted
    bool operator==(const FaceKey& o) const {
        return e0 == o.e0 && e1 == o.e1 && e2 == o.e2;
    }
};

struct FaceKeyHash {
    size_t operator()(const FaceKey& k) const {
        size_t h = std::hash<int>{}(k.e0);
        h ^= std::hash<int>{}(k.e1) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
        h ^= std::hash<int>{}(k.e2) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
        return h;
    }
};

static inline VertKey make_vert_key(double x, double y, double z) {
    return {
        std::llround(x * 10000.0),
        std::llround(y * 10000.0),
        std::llround(z * 10000.0)
    };
}


void build_simplicial_complex(
    const double* edge_verts, int num_edges,
    std::vector<std::array<int,3>>& faces_out,
    std::vector<int>& bnd_row,
    std::vector<int>& bnd_col,
    std::vector<int>& bnd_val)
{
    // Assign unique integer IDs to vertices
    std::unordered_map<VertKey, int, VertKeyHash> vert_ids;
    vert_ids.reserve(num_edges);  // rough estimate

    std::vector<std::array<int,2>> ev(num_edges);  // (v0_id, v1_id) per edge

    for (int i = 0; i < num_edges; ++i) {
        const double* p0 = edge_verts + i * 6;
        const double* p1 = p0 + 3;
        VertKey vk0 = make_vert_key(p0[0], p0[1], p0[2]);
        VertKey vk1 = make_vert_key(p1[0], p1[1], p1[2]);

        auto [it0, ins0] = vert_ids.try_emplace(vk0, (int)vert_ids.size());
        int id0 = it0->second;
        auto [it1, ins1] = vert_ids.try_emplace(vk1, (int)vert_ids.size());
        int id1 = it1->second;

        ev[i] = {id0, id1};
    }

    // Adjacency 
    // vert_to_edges[v] = list of (edge_index, other_vertex)
    std::unordered_map<int, std::vector<std::pair<int,int>>> vert_to_edges;
    vert_to_edges.reserve(vert_ids.size());

    // edge_lookup[(min,max)] = edge_index
    std::unordered_map<EdgeKey, int, EdgeKeyHash> edge_lookup;
    edge_lookup.reserve(num_edges);

    for (int i = 0; i < num_edges; ++i) {
        int v0 = ev[i][0], v1 = ev[i][1];
        vert_to_edges[v0].push_back({i, v1});
        vert_to_edges[v1].push_back({i, v0});
        int lo = std::min(v0, v1), hi = std::max(v0, v1);
        edge_lookup[{lo, hi}] = i;
    }

    // Find triangular faces
    std::unordered_set<FaceKey, FaceKeyHash> face_set;

    for (auto& [v, neighbors] : vert_to_edges) {
        int n = (int)neighbors.size();
        for (int a = 0; a < n; ++a) {
            for (int b = a + 1; b < n; ++b) {
                int va = neighbors[a].second;
                int vb = neighbors[b].second;
                int lo = std::min(va, vb), hi = std::max(va, vb);
                auto it = edge_lookup.find({lo, hi});
                if (it != edge_lookup.end()) {
                    std::array<int,3> se = {neighbors[a].first,
                                            neighbors[b].first,
                                            it->second};
                    std::sort(se.begin(), se.end());
                    face_set.insert({se[0], se[1], se[2]});
                }
            }
        }
    }

    faces_out.clear();
    faces_out.reserve(face_set.size());
    for (auto& fk : face_set)
        faces_out.push_back({fk.e0, fk.e1, fk.e2});
    std::sort(faces_out.begin(), faces_out.end());

    // Oriented boundary
    int num_faces = (int)faces_out.size();
    bnd_row.clear();  bnd_row.reserve(num_faces * 3);
    bnd_col.clear();  bnd_col.reserve(num_faces * 3);
    bnd_val.clear();  bnd_val.reserve(num_faces * 3);

    for (int f = 0; f < num_faces; ++f) {
        // Collect the 3 unique vertices of this triangle
        int vset[6];
        int nv = 0;
        for (int ei : faces_out[f]) {
            vset[nv++] = ev[ei][0];
            vset[nv++] = ev[ei][1];
        }
        std::sort(vset, vset + 6);
        int tri[3];
        int nt = 0;
        tri[nt++] = vset[0];
        for (int k = 1; k < 6; ++k)
            if (vset[k] != vset[k-1]) tri[nt++] = vset[k];
        if (nt != 3) continue;
        // tri[] is already sorted

        // Oriented cycle: tri[0]->tri[1], tri[1]->tri[2], tri[2]->tri[0]
        int cycle_src[3] = {tri[0], tri[1], tri[2]};
        int cycle_dst[3] = {tri[1], tri[2], tri[0]};

        for (int c = 0; c < 3; ++c) {
            int lo = std::min(cycle_src[c], cycle_dst[c]);
            int hi = std::max(cycle_src[c], cycle_dst[c]);
            auto it = edge_lookup.find({lo, hi});
            if (it != edge_lookup.end()) {
                int ei = it->second;
                int sign = (ev[ei][0] == cycle_src[c] &&
                            ev[ei][1] == cycle_dst[c]) ? 1 : -1;
                bnd_row.push_back(ei);
                bnd_col.push_back(f);
                bnd_val.push_back(sign);
            }
        }
    }
}
