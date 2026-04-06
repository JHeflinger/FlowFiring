#include "sparse.h"
#include <cstdint>
#include <vector>
#include <cuda_runtime.h>

// --- Kernels ---
__global__ void check_eligibility_kernel(
    const int32_t* config,
    const int32_t* deg,
    const int32_t* edges,
    int32_t* signs,
    int num_edges_in_class)
{
    int k = blockIdx.x * blockDim.x + threadIdx.x;
    if (k >= num_edges_in_class) return;

    int i = edges[k];
    if (deg[i] > 0 && ::abs(config[i]) >= deg[i]) {
        signs[k] = (config[i] > 0) ? 1 : -1;
    } else {
        signs[k] = 0;
    }
}

__global__ void apply_firings_kernel(
    int32_t* config,
    const int32_t* signs,
    const int32_t* edges,
    const int32_t* col_ptr,
    const int32_t* row_ind,
    const int32_t* vals,
    int num_edges_in_class)
{
    int k = blockIdx.x * blockDim.x + threadIdx.x;
    if (k >= num_edges_in_class) return;
    if (signs[k] == 0) return;

    int i = edges[k];
    int32_t s = signs[k];

    int col_start = col_ptr[i];
    int col_end = col_ptr[i + 1];
    for (int p = col_start; p < col_end; ++p) {
        int j = row_ind[p];
        int32_t val = s * vals[p];
        atomicSub(&config[j], val);
    }
}

// Count nonzero signs (fired edges)
__global__ void count_fired_kernel(
    const int32_t* signs,
    int num_edges_in_class,
    int* count)
{
    int k = blockIdx.x * blockDim.x + threadIdx.x;
    if (k >= num_edges_in_class) return;
    if (signs[k] != 0) {
        atomicAdd(count, 1);
    }
}


// --- Gather kernels ---

// Check eligibility for one color class, write signs into global array
// (indexed by global edge id, not color-class-local index)
__global__ void check_eligibility_global_kernel(
    const int32_t* config,
    const int32_t* deg,
    const int32_t* edges,
    int32_t* signs_global,   // length = num_edges (all edges)
    int num_edges_in_class)
{
    int k = blockIdx.x * blockDim.x + threadIdx.x;
    if (k >= num_edges_in_class) return;

    int i = edges[k];
    if (deg[i] > 0 && ::abs(config[i]) >= deg[i]) {
        signs_global[i] = (config[i] > 0) ? 1 : -1;
    } else {
        signs_global[i] = 0;
    }
}

// Each edge reads from its neighbors to accumulate updates
__global__ void gather_updates_kernel(
    int32_t* config,
    const int32_t* signs_global,  // indexed by global edge id
    const int32_t* col_ptr,
    const int32_t* row_ind,
    const int32_t* vals,
    int num_edges)
{
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= num_edges) return;

    int32_t delta = 0;
    int start = col_ptr[i];
    int end = col_ptr[i + 1];
    for (int p = start; p < end; ++p) {
        int j = row_ind[p];
        int32_t s = signs_global[j];
        if (s != 0) {
            delta += s * vals[p];
        }
    }
    if (delta != 0)
        config[i] -= delta;
}

// Count nonzero entries in signs_global for edges in a color class
__global__ void count_fired_global_kernel(
    const int32_t* signs_global,
    const int32_t* edges,
    int num_edges_in_class,
    int* count)
{
    int k = blockIdx.x * blockDim.x + threadIdx.x;
    if (k >= num_edges_in_class) return;
    if (signs_global[edges[k]] != 0) {
        atomicAdd(count, 1);
    }
}


// --- Host API ---

extern "C" int fire_step_cuda(
    int32_t* config,
    const int32_t* deg,
    const int32_t* col_ptr,
    const int32_t* row_ind,
    const int32_t* vals,
    int num_edges,
    const int32_t* color_offsets,
    const int32_t* color_edges,
    int num_colors)
{
    // Allocate device memory
    int32_t *d_config, *d_deg, *d_col_ptr, *d_row_ind, *d_vals;
    int32_t *d_color_edges, *d_signs;
    int *d_count;

    // Compute total nnz from col_ptr
    int nnz;
    {
        int last_ptr;
        // col_ptr is host memory here, read last element
        last_ptr = col_ptr[num_edges];
        nnz = last_ptr;
    }

    int total_color_edges = color_offsets[num_colors];

    cudaMalloc(&d_config, num_edges * sizeof(int32_t));
    cudaMalloc(&d_deg, num_edges * sizeof(int32_t));
    cudaMalloc(&d_col_ptr, (num_edges + 1) * sizeof(int32_t));
    cudaMalloc(&d_row_ind, nnz * sizeof(int32_t));
    cudaMalloc(&d_vals, nnz * sizeof(int32_t));
    cudaMalloc(&d_color_edges, total_color_edges * sizeof(int32_t));
    cudaMalloc(&d_signs, total_color_edges * sizeof(int32_t));
    cudaMalloc(&d_count, sizeof(int));

    cudaMemcpy(d_config, config, num_edges * sizeof(int32_t), cudaMemcpyHostToDevice);
    cudaMemcpy(d_deg, deg, num_edges * sizeof(int32_t), cudaMemcpyHostToDevice);
    cudaMemcpy(d_col_ptr, col_ptr, (num_edges + 1) * sizeof(int32_t), cudaMemcpyHostToDevice);
    cudaMemcpy(d_row_ind, row_ind, nnz * sizeof(int32_t), cudaMemcpyHostToDevice);
    cudaMemcpy(d_vals, vals, nnz * sizeof(int32_t), cudaMemcpyHostToDevice);
    cudaMemcpy(d_color_edges, color_edges, total_color_edges * sizeof(int32_t), cudaMemcpyHostToDevice);

    int total_fired = 0;
    int h_count = 0;
    cudaMemset(d_count, 0, sizeof(int));

    const int block_size = 256;

    for (int c = 0; c < num_colors; ++c) {
        int start = color_offsets[c];
        int count = color_offsets[c + 1] - start;
        if (count == 0) continue;

        int num_blocks = (count + block_size - 1) / block_size;

        check_eligibility_kernel<<<num_blocks, block_size>>>(
            d_config, d_deg, d_color_edges + start, d_signs + start, count);

        apply_firings_kernel<<<num_blocks, block_size>>>(
            d_config, d_signs + start, d_color_edges + start,
            d_col_ptr, d_row_ind, d_vals, count);

        // Implicit sync between kernel launches on same stream
        cudaMemset(d_count, 0, sizeof(int));
        count_fired_kernel<<<num_blocks, block_size>>>(
            d_signs + start, count, d_count);
        cudaMemcpy(&h_count, d_count, sizeof(int), cudaMemcpyDeviceToHost);
        total_fired += h_count;
    }

    // Copy result back
    cudaMemcpy(config, d_config, num_edges * sizeof(int32_t), cudaMemcpyDeviceToHost);

    cudaFree(d_config);
    cudaFree(d_deg);
    cudaFree(d_col_ptr);
    cudaFree(d_row_ind);
    cudaFree(d_vals);
    cudaFree(d_color_edges);
    cudaFree(d_signs);
    cudaFree(d_count);

    return total_fired;
}

extern "C" bool has_cuda_runtime() {
    int device_count = 0;
    cudaError_t err = cudaGetDeviceCount(&device_count);
    return (err == cudaSuccess && device_count > 0);
}


// --- Persistent Session ---

class CudaFiringSession {
    int32_t *d_config = nullptr;
    int32_t *d_deg = nullptr;
    int32_t *d_col_ptr = nullptr;
    int32_t *d_row_ind = nullptr;
    int32_t *d_vals = nullptr;
    int32_t *d_color_edges = nullptr;
    int32_t *d_signs = nullptr;
    int *d_count = nullptr;

    int num_edges;
    int num_colors;
    int total_color_edges;
    std::vector<int32_t> h_color_offsets;

public:
    CudaFiringSession(
        const int32_t* config,
        const int32_t* deg,
        const int32_t* col_ptr,
        const int32_t* row_ind,
        const int32_t* vals,
        int num_edges,
        const int32_t* color_offsets,
        const int32_t* color_edges,
        int num_colors)
        : num_edges(num_edges), num_colors(num_colors)
    {
        int nnz = col_ptr[num_edges];
        total_color_edges = color_offsets[num_colors];

        // Store color offsets on host
        h_color_offsets.assign(color_offsets, color_offsets + num_colors + 1);

        cudaMalloc(&d_config, num_edges * sizeof(int32_t));
        cudaMalloc(&d_deg, num_edges * sizeof(int32_t));
        cudaMalloc(&d_col_ptr, (num_edges + 1) * sizeof(int32_t));
        cudaMalloc(&d_row_ind, nnz * sizeof(int32_t));
        cudaMalloc(&d_vals, nnz * sizeof(int32_t));
        cudaMalloc(&d_color_edges, total_color_edges * sizeof(int32_t));
        cudaMalloc(&d_signs, total_color_edges * sizeof(int32_t));
        cudaMalloc(&d_count, sizeof(int));

        cudaMemcpy(d_config, config, num_edges * sizeof(int32_t), cudaMemcpyHostToDevice);
        cudaMemcpy(d_deg, deg, num_edges * sizeof(int32_t), cudaMemcpyHostToDevice);
        cudaMemcpy(d_col_ptr, col_ptr, (num_edges + 1) * sizeof(int32_t), cudaMemcpyHostToDevice);
        cudaMemcpy(d_row_ind, row_ind, nnz * sizeof(int32_t), cudaMemcpyHostToDevice);
        cudaMemcpy(d_vals, vals, nnz * sizeof(int32_t), cudaMemcpyHostToDevice);
        cudaMemcpy(d_color_edges, color_edges, total_color_edges * sizeof(int32_t), cudaMemcpyHostToDevice);
    }

    int step() {
        const int block_size = 256;
        int total_fired = 0;
        int h_count = 0;

        for (int c = 0; c < num_colors; ++c) {
            int start = h_color_offsets[c];
            int count = h_color_offsets[c + 1] - start;
            if (count == 0) continue;

            int num_blocks = (count + block_size - 1) / block_size;

            check_eligibility_kernel<<<num_blocks, block_size>>>(
                d_config, d_deg, d_color_edges + start, d_signs + start, count);

            apply_firings_kernel<<<num_blocks, block_size>>>(
                d_config, d_signs + start, d_color_edges + start,
                d_col_ptr, d_row_ind, d_vals, count);

            cudaMemset(d_count, 0, sizeof(int));
            count_fired_kernel<<<num_blocks, block_size>>>(
                d_signs + start, count, d_count);
            cudaMemcpy(&h_count, d_count, sizeof(int), cudaMemcpyDeviceToHost);
            total_fired += h_count;
        }

        return total_fired;
    }

    void read_config(int32_t* host) {
        cudaMemcpy(host, d_config, num_edges * sizeof(int32_t), cudaMemcpyDeviceToHost);
    }

    ~CudaFiringSession() {
        cudaFree(d_config);
        cudaFree(d_deg);
        cudaFree(d_col_ptr);
        cudaFree(d_row_ind);
        cudaFree(d_vals);
        cudaFree(d_color_edges);
        cudaFree(d_signs);
        cudaFree(d_count);
    }
};


// --- Gather-based persistent session ---

class CudaGatherSession {
    int32_t *d_config = nullptr;
    int32_t *d_deg = nullptr;
    int32_t *d_col_ptr = nullptr;
    int32_t *d_row_ind = nullptr;
    int32_t *d_vals = nullptr;
    int32_t *d_color_edges = nullptr;
    int32_t *d_signs_global = nullptr;  // length = num_edges
    int *d_count = nullptr;

    int num_edges;
    int num_colors;
    std::vector<int32_t> h_color_offsets;

public:
    CudaGatherSession(
        const int32_t* config, const int32_t* deg,
        const int32_t* col_ptr, const int32_t* row_ind, const int32_t* vals,
        int num_edges, const int32_t* color_offsets, const int32_t* color_edges,
        int num_colors)
        : num_edges(num_edges), num_colors(num_colors)
    {
        int nnz = col_ptr[num_edges];
        int total_color_edges = color_offsets[num_colors];
        h_color_offsets.assign(color_offsets, color_offsets + num_colors + 1);

        cudaMalloc(&d_config, num_edges * sizeof(int32_t));
        cudaMalloc(&d_deg, num_edges * sizeof(int32_t));
        cudaMalloc(&d_col_ptr, (num_edges + 1) * sizeof(int32_t));
        cudaMalloc(&d_row_ind, nnz * sizeof(int32_t));
        cudaMalloc(&d_vals, nnz * sizeof(int32_t));
        cudaMalloc(&d_color_edges, total_color_edges * sizeof(int32_t));
        cudaMalloc(&d_signs_global, num_edges * sizeof(int32_t));
        cudaMalloc(&d_count, sizeof(int));

        cudaMemcpy(d_config, config, num_edges * sizeof(int32_t), cudaMemcpyHostToDevice);
        cudaMemcpy(d_deg, deg, num_edges * sizeof(int32_t), cudaMemcpyHostToDevice);
        cudaMemcpy(d_col_ptr, col_ptr, (num_edges + 1) * sizeof(int32_t), cudaMemcpyHostToDevice);
        cudaMemcpy(d_row_ind, row_ind, nnz * sizeof(int32_t), cudaMemcpyHostToDevice);
        cudaMemcpy(d_vals, vals, nnz * sizeof(int32_t), cudaMemcpyHostToDevice);
        cudaMemcpy(d_color_edges, color_edges, total_color_edges * sizeof(int32_t), cudaMemcpyHostToDevice);
        cudaMemset(d_signs_global, 0, num_edges * sizeof(int32_t));
    }

    int step() {
        const int block_size = 256;
        int total_fired = 0;
        int h_count = 0;

        for (int c = 0; c < num_colors; ++c) {
            int start = h_color_offsets[c];
            int count = h_color_offsets[c + 1] - start;
            if (count == 0) continue;

            int class_blocks = (count + block_size - 1) / block_size;
            int all_blocks = (num_edges + block_size - 1) / block_size;

            // Clear signs
            cudaMemset(d_signs_global, 0, num_edges * sizeof(int32_t));

            // Check eligibility, write signs to global array
            check_eligibility_global_kernel<<<class_blocks, block_size>>>(
                d_config, d_deg, d_color_edges + start,
                d_signs_global, count);

            // Gather
            gather_updates_kernel<<<all_blocks, block_size>>>(
                d_config, d_signs_global,
                d_col_ptr, d_row_ind, d_vals, num_edges);

            // Count fired
            cudaMemset(d_count, 0, sizeof(int));
            count_fired_global_kernel<<<class_blocks, block_size>>>(
                d_signs_global, d_color_edges + start, count, d_count);
            cudaMemcpy(&h_count, d_count, sizeof(int), cudaMemcpyDeviceToHost);
            total_fired += h_count;
        }

        return total_fired;
    }

    void read_config(int32_t* host) {
        cudaMemcpy(host, d_config, num_edges * sizeof(int32_t), cudaMemcpyDeviceToHost);
    }

    ~CudaGatherSession() {
        cudaFree(d_config);
        cudaFree(d_deg);
        cudaFree(d_col_ptr);
        cudaFree(d_row_ind);
        cudaFree(d_vals);
        cudaFree(d_color_edges);
        cudaFree(d_signs_global);
        cudaFree(d_count);
    }
};


// --- Session C API ---

extern "C" void* cuda_session_create(
    const int32_t* config,
    const int32_t* deg,
    const int32_t* col_ptr,
    const int32_t* row_ind,
    const int32_t* vals,
    int num_edges,
    const int32_t* color_offsets,
    const int32_t* color_edges,
    int num_colors)
{
    return new CudaFiringSession(
        config, deg, col_ptr, row_ind, vals,
        num_edges, color_offsets, color_edges, num_colors);
}

extern "C" int cuda_session_step(void* session) {
    return static_cast<CudaFiringSession*>(session)->step();
}

extern "C" void cuda_session_read_config(void* session, int32_t* host) {
    static_cast<CudaFiringSession*>(session)->read_config(host);
}

extern "C" void cuda_session_destroy(void* session) {
    delete static_cast<CudaFiringSession*>(session);
}

// Gather session C API
extern "C" void* cuda_gather_session_create(
    const int32_t* config, const int32_t* deg,
    const int32_t* col_ptr, const int32_t* row_ind, const int32_t* vals,
    int num_edges, const int32_t* color_offsets, const int32_t* color_edges,
    int num_colors)
{
    return new CudaGatherSession(
        config, deg, col_ptr, row_ind, vals,
        num_edges, color_offsets, color_edges, num_colors);
}

extern "C" int cuda_gather_session_step(void* session) {
    return static_cast<CudaGatherSession*>(session)->step();
}

extern "C" void cuda_gather_session_read_config(void* session, int32_t* host) {
    static_cast<CudaGatherSession*>(session)->read_config(host);
}

extern "C" void cuda_gather_session_destroy(void* session) {
    delete static_cast<CudaGatherSession*>(session);
}

extern "C" int fire_colored_cuda(
    int32_t* config,
    const int32_t* deg,
    const int32_t* col_ptr,
    const int32_t* row_ind,
    const int32_t* vals,
    int num_edges,
    const int32_t* color_offsets,
    const int32_t* color_edges,
    int num_colors,
    int max_steps)
{
    CudaFiringSession session(
        config, deg, col_ptr, row_ind, vals,
        num_edges, color_offsets, color_edges, num_colors);
    int total_fired = 0;
    for (int step = 0; step < max_steps; ++step) {
        int fired = session.step();
        total_fired += fired;
        if (fired == 0) break;
    }
    session.read_config(config);
    return total_fired;
}
