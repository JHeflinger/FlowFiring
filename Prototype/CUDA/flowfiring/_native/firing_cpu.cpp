#include "sparse.h"
#include <cstdlib>
#include <cstdint>
#include <vector>
#include <numeric>
#include <random>
#include <algorithm>

#ifdef _OPENMP
#include <omp.h>
#endif


// Sequential firing
int fire_sequential_step_cpu(
    int32_t* config,
    const int32_t* deg,
    const int32_t* col_ptr,
    const int32_t* row_ind,
    const int32_t* vals,
    int num_edges,
    const int32_t* order,
    int order_len)
{
    CSCMatrix L;
    L.indptr = col_ptr;
    L.indices = row_ind;
    L.data = vals;
    L.num_cols = num_edges;

    int fired = 0;
    for (int k = 0; k < order_len; ++k) {
        int i = order[k];
        if (deg[i] > 0 && std::abs(config[i]) >= deg[i]) {
            int32_t sign = (config[i] > 0) ? 1 : -1;
            int col_start = L.indptr[i];
            int col_end = L.indptr[i + 1];
            for (int p = col_start; p < col_end; ++p) {
                config[L.indices[p]] -= sign * L.data[p];
            }
            fired++;
        }
    }
    return fired;
}


// Colored (parallel) firing
static int fire_color_class_cpu(
    int32_t* config,
    const int32_t* deg,
    const CSCMatrix& L,
    const int32_t* edges,
    int num_edges_in_class)
{
    // Check eligibility, compute signs
    std::vector<int32_t> signs(num_edges_in_class, 0);

    #ifdef _OPENMP
    #pragma omp parallel for schedule(static)
    #endif
    for (int k = 0; k < num_edges_in_class; ++k) {
        int i = edges[k];
        if (deg[i] > 0 && std::abs(config[i]) >= deg[i]) {
            signs[k] = (config[i] > 0) ? 1 : -1;
        }
    }

    // Scatter with atomics
    int fired = 0;

    #ifdef _OPENMP
    #pragma omp parallel for schedule(static) reduction(+:fired)
    #endif
    for (int k = 0; k < num_edges_in_class; ++k) {
        if (signs[k] == 0) continue;
        fired += 1;
        int i = edges[k];
        int32_t s = signs[k];

        int col_start = L.indptr[i];
        int col_end = L.indptr[i + 1];
        for (int p = col_start; p < col_end; ++p) {
            int j = L.indices[p];
            int32_t val = s * L.data[p];
            #ifdef _OPENMP
            #pragma omp atomic
            #endif
            config[j] -= val;
        }
    }

    return fired;
}

int fire_step_cpu(
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
    CSCMatrix L;
    L.indptr = col_ptr;
    L.indices = row_ind;
    L.data = vals;
    L.num_cols = num_edges;

    int total_fired = 0;
    for (int c = 0; c < num_colors; ++c) {
        int start = color_offsets[c];
        int count = color_offsets[c + 1] - start;
        total_fired += fire_color_class_cpu(
            config, deg, L, color_edges + start, count);
    }
    return total_fired;
}


// Multi-step sequential firing with optional shuffle.
int fire_sequential_cpu(
    int32_t* config,
    const int32_t* deg,
    const int32_t* col_ptr,
    const int32_t* row_ind,
    const int32_t* vals,
    int num_edges,
    int max_steps,
    bool shuffle,
    int64_t seed)
{
    std::vector<int32_t> order(num_edges);
    std::iota(order.begin(), order.end(), 0);

    std::mt19937 rng;
    if (shuffle)
        rng.seed(static_cast<unsigned>(seed));

    int total_fired = 0;
    for (int step = 0; step < max_steps; ++step) {
        if (shuffle)
            std::shuffle(order.begin(), order.end(), rng);
        int fired = fire_sequential_step_cpu(
            config, deg, col_ptr, row_ind, vals,
            num_edges, order.data(), num_edges);
        total_fired += fired;
        if (fired == 0) break;
    }
    return total_fired;
}


// Multi-step colored firing (CPU).
int fire_colored_cpu(
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
    int total_fired = 0;
    for (int step = 0; step < max_steps; ++step) {
        int fired = fire_step_cpu(
            config, deg, col_ptr, row_ind, vals,
            num_edges, color_offsets, color_edges, num_colors);
        total_fired += fired;
        if (fired == 0) break;
    }
    return total_fired;
}
