#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <cstdint>

namespace py = pybind11;

// Builders (builders.cpp)
extern py::dict build_lattice_3d_full(int size, bool with_colors);
extern py::dict build_grid_2d_full(int rows, int cols,
                                   bool has_hollow, int x_lo, int y_lo, int x_hi, int y_hi,
                                   bool with_colors);
extern py::dict build_grid_2d_quad_full(int rows, int cols,
                                        bool has_hollow, int x_lo, int y_lo, int x_hi, int y_hi,
                                        bool with_colors);
extern py::dict build_grid_3d_full(int nx, int ny, int nz, bool with_colors,
                                   py::array_t<int32_t> hollow_face_planes,
                                   py::array_t<int32_t> hollow_face_coords);

// CPU firing (firing_cpu.cpp)
extern int fire_sequential_step_cpu(
    int32_t* config, const int32_t* deg,
    const int32_t* col_ptr, const int32_t* row_ind, const int32_t* vals,
    int num_edges, const int32_t* order, int order_len);

extern int fire_step_cpu(
    int32_t* config, const int32_t* deg,
    const int32_t* col_ptr, const int32_t* row_ind, const int32_t* vals,
    int num_edges, const int32_t* color_offsets, const int32_t* color_edges,
    int num_colors);

extern int fire_sequential_cpu(
    int32_t* config, const int32_t* deg,
    const int32_t* col_ptr, const int32_t* row_ind, const int32_t* vals,
    int num_edges, int max_steps, bool shuffle, int64_t seed);

extern int fire_colored_cpu(
    int32_t* config, const int32_t* deg,
    const int32_t* col_ptr, const int32_t* row_ind, const int32_t* vals,
    int num_edges, const int32_t* color_offsets, const int32_t* color_edges,
    int num_colors, int max_steps);

// Coloring (coloring.cpp)
extern void color_conflict_graph_native(
    const int32_t* indptr, const int32_t* indices, int n, int32_t* colors_out);

// CUDA (optional)
#ifdef WITH_CUDA
extern "C" int fire_step_cuda(
    int32_t* config, const int32_t* deg,
    const int32_t* col_ptr, const int32_t* row_ind, const int32_t* vals,
    int num_edges, const int32_t* color_offsets, const int32_t* color_edges,
    int num_colors);

extern "C" int fire_colored_cuda(
    int32_t* config, const int32_t* deg,
    const int32_t* col_ptr, const int32_t* row_ind, const int32_t* vals,
    int num_edges, const int32_t* color_offsets, const int32_t* color_edges,
    int num_colors, int max_steps);

extern "C" bool has_cuda_runtime();

extern "C" void* cuda_session_create(
    const int32_t* config, const int32_t* deg,
    const int32_t* col_ptr, const int32_t* row_ind, const int32_t* vals,
    int num_edges, const int32_t* color_offsets, const int32_t* color_edges,
    int num_colors);
extern "C" int   cuda_session_step(void* session);
extern "C" void  cuda_session_read_config(void* session, int32_t* host);
extern "C" void  cuda_session_destroy(void* session);

extern "C" void* cuda_gather_session_create(
    const int32_t* config, const int32_t* deg,
    const int32_t* col_ptr, const int32_t* row_ind, const int32_t* vals,
    int num_edges, const int32_t* color_offsets, const int32_t* color_edges,
    int num_colors);
extern "C" int   cuda_gather_session_step(void* session);
extern "C" void  cuda_gather_session_read_config(void* session, int32_t* host);
extern "C" void  cuda_gather_session_destroy(void* session);
#endif

// ---------------------------------------------------------------------------
// Binding helpers
// ---------------------------------------------------------------------------

static int fire_sequential_step_binding(
    py::array_t<int32_t, py::array::c_style> config,
    py::array_t<int32_t, py::array::c_style> deg,
    py::array_t<int32_t, py::array::c_style> col_ptr,
    py::array_t<int32_t, py::array::c_style> row_ind,
    py::array_t<int32_t, py::array::c_style> vals,
    py::array_t<int32_t, py::array::c_style> order)
{
    auto cfg = config.mutable_unchecked<1>();
    int num_edges = (int)cfg.shape(0);
    return fire_sequential_step_cpu(
        cfg.mutable_data(0),
        deg.unchecked<1>().data(0),
        col_ptr.unchecked<1>().data(0),
        row_ind.unchecked<1>().data(0),
        vals.unchecked<1>().data(0),
        num_edges,
        order.unchecked<1>().data(0),
        (int)order.size());
}

static int fire_sequential_binding(
    py::array_t<int32_t, py::array::c_style> config,
    py::array_t<int32_t, py::array::c_style> deg,
    py::array_t<int32_t, py::array::c_style> col_ptr,
    py::array_t<int32_t, py::array::c_style> row_ind,
    py::array_t<int32_t, py::array::c_style> vals,
    int max_steps, bool shuffle, int64_t seed)
{
    auto cfg = config.mutable_unchecked<1>();
    int num_edges = (int)cfg.shape(0);
    return fire_sequential_cpu(
        cfg.mutable_data(0),
        deg.unchecked<1>().data(0),
        col_ptr.unchecked<1>().data(0),
        row_ind.unchecked<1>().data(0),
        vals.unchecked<1>().data(0),
        num_edges, max_steps, shuffle, seed);
}

static int fire_step_binding(
    py::array_t<int32_t, py::array::c_style> config,
    py::array_t<int32_t, py::array::c_style> deg,
    py::array_t<int32_t, py::array::c_style> col_ptr,
    py::array_t<int32_t, py::array::c_style> row_ind,
    py::array_t<int32_t, py::array::c_style> vals,
    py::array_t<int32_t, py::array::c_style> color_offsets,
    py::array_t<int32_t, py::array::c_style> color_edges,
    bool use_cuda)
{
    auto cfg = config.mutable_unchecked<1>();
    int num_edges = (int)cfg.shape(0);
    auto co = color_offsets.unchecked<1>();
    int num_colors = (int)co.shape(0) - 1;

#ifdef WITH_CUDA
    if (use_cuda) {
        return fire_step_cuda(
            cfg.mutable_data(0),
            deg.unchecked<1>().data(0),
            col_ptr.unchecked<1>().data(0),
            row_ind.unchecked<1>().data(0),
            vals.unchecked<1>().data(0),
            num_edges,
            co.data(0),
            color_edges.unchecked<1>().data(0),
            num_colors);
    }
#endif

    return fire_step_cpu(
        cfg.mutable_data(0),
        deg.unchecked<1>().data(0),
        col_ptr.unchecked<1>().data(0),
        row_ind.unchecked<1>().data(0),
        vals.unchecked<1>().data(0),
        num_edges,
        co.data(0),
        color_edges.unchecked<1>().data(0),
        num_colors);
}

static int fire_colored_binding(
    py::array_t<int32_t, py::array::c_style> config,
    py::array_t<int32_t, py::array::c_style> deg,
    py::array_t<int32_t, py::array::c_style> col_ptr,
    py::array_t<int32_t, py::array::c_style> row_ind,
    py::array_t<int32_t, py::array::c_style> vals,
    py::array_t<int32_t, py::array::c_style> color_offsets,
    py::array_t<int32_t, py::array::c_style> color_edges,
    int max_steps, bool use_cuda)
{
    auto cfg = config.mutable_unchecked<1>();
    int num_edges = (int)cfg.shape(0);
    auto co = color_offsets.unchecked<1>();
    int num_colors = (int)co.shape(0) - 1;

#ifdef WITH_CUDA
    if (use_cuda) {
        return fire_colored_cuda(
            cfg.mutable_data(0),
            deg.unchecked<1>().data(0),
            col_ptr.unchecked<1>().data(0),
            row_ind.unchecked<1>().data(0),
            vals.unchecked<1>().data(0),
            num_edges,
            co.data(0),
            color_edges.unchecked<1>().data(0),
            num_colors, max_steps);
    }
#endif

    return fire_colored_cpu(
        cfg.mutable_data(0),
        deg.unchecked<1>().data(0),
        col_ptr.unchecked<1>().data(0),
        row_ind.unchecked<1>().data(0),
        vals.unchecked<1>().data(0),
        num_edges,
        co.data(0),
        color_edges.unchecked<1>().data(0),
        num_colors, max_steps);
}

static bool has_cuda() {
#ifdef WITH_CUDA
    return has_cuda_runtime();
#else
    return false;
#endif
}

static py::array_t<int32_t> color_cg_binding(
    py::array_t<int32_t, py::array::c_style> indptr,
    py::array_t<int32_t, py::array::c_style> indices,
    int n)
{
    auto colors = py::array_t<int32_t>(n);
    color_conflict_graph_native(
        indptr.unchecked<1>().data(0),
        indices.unchecked<1>().data(0),
        n, colors.mutable_data());
    return colors;
}

// ---------------------------------------------------------------------------
// CUDA session wrapper
// ---------------------------------------------------------------------------

#ifdef WITH_CUDA
class CudaFiringSessionWrapper {
    void* session;
public:
    CudaFiringSessionWrapper(
        const int32_t* config, const int32_t* deg,
        const int32_t* col_ptr, const int32_t* row_ind, const int32_t* vals,
        int num_edges, const int32_t* color_offsets, const int32_t* color_edges,
        int num_colors)
    {
        session = cuda_session_create(
            config, deg, col_ptr, row_ind, vals,
            num_edges, color_offsets, color_edges, num_colors);
    }

    int step() { return cuda_session_step(session); }

    void read_config(py::array_t<int32_t, py::array::c_style> config) {
        cuda_session_read_config(session, config.mutable_data());
    }

    ~CudaFiringSessionWrapper() {
        if (session) { cuda_session_destroy(session); session = nullptr; }
    }
};
#endif

// ---------------------------------------------------------------------------
// Module definition
// ---------------------------------------------------------------------------

PYBIND11_MODULE(_native, m) {
    m.doc() = "Native firing kernels and builders for flowfiring";

    // Builders
    m.def("build_lattice_3d", &build_lattice_3d_full,
          py::arg("size") = 10, py::arg("with_colors") = false);
    m.def("build_grid_2d", &build_grid_2d_full,
          py::arg("rows"), py::arg("cols"),
          py::arg("has_hollow") = false,
          py::arg("x_lo") = 0, py::arg("y_lo") = 0,
          py::arg("x_hi") = 0, py::arg("y_hi") = 0,
          py::arg("with_colors") = false);
    m.def("build_grid_2d_quad", &build_grid_2d_quad_full,
          py::arg("rows"), py::arg("cols"),
          py::arg("has_hollow") = false,
          py::arg("x_lo") = 0, py::arg("y_lo") = 0,
          py::arg("x_hi") = 0, py::arg("y_hi") = 0,
          py::arg("with_colors") = false);
    m.def("build_grid_3d", &build_grid_3d_full,
          py::arg("nx"), py::arg("ny"), py::arg("nz"),
          py::arg("with_colors") = false,
          py::arg("hollow_face_planes") = py::array_t<int32_t>(0),
          py::arg("hollow_face_coords") = py::array_t<int32_t>(0));

    // Single-step firing
    m.def("fire_sequential_step", &fire_sequential_step_binding,
          py::arg("config"), py::arg("deg"),
          py::arg("col_ptr"), py::arg("row_ind"), py::arg("vals"),
          py::arg("order"));
    m.def("fire_step", &fire_step_binding,
          py::arg("config"), py::arg("deg"),
          py::arg("col_ptr"), py::arg("row_ind"), py::arg("vals"),
          py::arg("color_offsets"), py::arg("color_edges"),
          py::arg("use_cuda") = false);

    // Multi-step firing
    m.def("fire_sequential", &fire_sequential_binding,
          py::arg("config"), py::arg("deg"),
          py::arg("col_ptr"), py::arg("row_ind"), py::arg("vals"),
          py::arg("max_steps") = 1000,
          py::arg("shuffle") = false, py::arg("seed") = 0);
    m.def("fire_colored", &fire_colored_binding,
          py::arg("config"), py::arg("deg"),
          py::arg("col_ptr"), py::arg("row_ind"), py::arg("vals"),
          py::arg("color_offsets"), py::arg("color_edges"),
          py::arg("max_steps") = 1000, py::arg("use_cuda") = false);

    // Utilities
    m.def("has_cuda", &has_cuda);
    m.def("color_conflict_graph", &color_cg_binding,
          py::arg("indptr"), py::arg("indices"), py::arg("n"));

#ifdef WITH_CUDA
    py::class_<CudaFiringSessionWrapper>(m, "CudaFiringSession")
        .def(py::init([](
                py::array_t<int32_t, py::array::c_style> config,
                py::array_t<int32_t, py::array::c_style> deg,
                py::array_t<int32_t, py::array::c_style> col_ptr,
                py::array_t<int32_t, py::array::c_style> row_ind,
                py::array_t<int32_t, py::array::c_style> vals,
                py::array_t<int32_t, py::array::c_style> color_offsets,
                py::array_t<int32_t, py::array::c_style> color_edges) {
            auto cfg = config.unchecked<1>();
            int num_edges = (int)cfg.shape(0);
            int num_colors = (int)color_offsets.unchecked<1>().shape(0) - 1;
            return new CudaFiringSessionWrapper(
                cfg.data(0), deg.unchecked<1>().data(0),
                col_ptr.unchecked<1>().data(0), row_ind.unchecked<1>().data(0),
                vals.unchecked<1>().data(0), num_edges,
                color_offsets.unchecked<1>().data(0),
                color_edges.unchecked<1>().data(0), num_colors);
        }),
             py::arg("config"), py::arg("deg"),
             py::arg("col_ptr"), py::arg("row_ind"), py::arg("vals"),
             py::arg("color_offsets"), py::arg("color_edges"))
        .def("step", &CudaFiringSessionWrapper::step)
        .def("read_config", &CudaFiringSessionWrapper::read_config);

    // Gather session (experiment)
    class CudaGatherSessionWrapper {
        void* session;
    public:
        CudaGatherSessionWrapper(
            const int32_t* config, const int32_t* deg,
            const int32_t* col_ptr, const int32_t* row_ind, const int32_t* vals,
            int num_edges, const int32_t* color_offsets, const int32_t* color_edges,
            int num_colors)
        {
            session = cuda_gather_session_create(
                config, deg, col_ptr, row_ind, vals,
                num_edges, color_offsets, color_edges, num_colors);
        }
        int step() { return cuda_gather_session_step(session); }
        void read_config(py::array_t<int32_t, py::array::c_style> config) {
            cuda_gather_session_read_config(session, config.mutable_data());
        }
        ~CudaGatherSessionWrapper() {
            if (session) { cuda_gather_session_destroy(session); session = nullptr; }
        }
    };

    py::class_<CudaGatherSessionWrapper>(m, "CudaGatherSession")
        .def(py::init([](
                py::array_t<int32_t, py::array::c_style> config,
                py::array_t<int32_t, py::array::c_style> deg,
                py::array_t<int32_t, py::array::c_style> col_ptr,
                py::array_t<int32_t, py::array::c_style> row_ind,
                py::array_t<int32_t, py::array::c_style> vals,
                py::array_t<int32_t, py::array::c_style> color_offsets,
                py::array_t<int32_t, py::array::c_style> color_edges) {
            auto cfg = config.unchecked<1>();
            int num_edges = (int)cfg.shape(0);
            int num_colors = (int)color_offsets.unchecked<1>().shape(0) - 1;
            return new CudaGatherSessionWrapper(
                cfg.data(0), deg.unchecked<1>().data(0),
                col_ptr.unchecked<1>().data(0), row_ind.unchecked<1>().data(0),
                vals.unchecked<1>().data(0), num_edges,
                color_offsets.unchecked<1>().data(0),
                color_edges.unchecked<1>().data(0), num_colors);
        }),
             py::arg("config"), py::arg("deg"),
             py::arg("col_ptr"), py::arg("row_ind"), py::arg("vals"),
             py::arg("color_offsets"), py::arg("color_edges"))
        .def("step", &CudaGatherSessionWrapper::step)
        .def("read_config", &CudaGatherSessionWrapper::read_config);
#endif
}
