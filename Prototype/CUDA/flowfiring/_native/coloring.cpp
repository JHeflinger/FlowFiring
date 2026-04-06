#include <cstdint>
#include <vector>
#include <algorithm>

void color_conflict_graph_native(
    const int32_t* indptr,
    const int32_t* indices,
    int n,
    int32_t* colors_out)
{
    // Build symmetric adjacency from CSC Laplacian
    // Count degrees
    std::vector<int> deg(n, 0);
    for (int j = 0; j < n; ++j) {
        for (int p = indptr[j]; p < indptr[j + 1]; ++p) {
            int i = indices[p];
            if (i != j) {  // skip diagonal
                ++deg[i];
                ++deg[j];
            }
        }
    }
    std::fill(deg.begin(), deg.end(), 0);
    for (int j = 0; j < n; ++j) {
        for (int p = indptr[j]; p < indptr[j + 1]; ++p) {
            int i = indices[p];
            if (i < j) {  // upper triangle only
                ++deg[i];
                ++deg[j];
            }
        }
    }

    // Build CSR adjacency
    std::vector<int> adj_ptr(n + 1, 0);
    for (int v = 0; v < n; ++v)
        adj_ptr[v + 1] = adj_ptr[v] + deg[v];
    int total_adj = adj_ptr[n];

    std::vector<int> adj_list(total_adj);
    std::vector<int> fill(n, 0);  // current fill position per vertex
    for (int j = 0; j < n; ++j) {
        for (int p = indptr[j]; p < indptr[j + 1]; ++p) {
            int i = indices[p];
            if (i < j) {
                adj_list[adj_ptr[i] + fill[i]++] = j;
                adj_list[adj_ptr[j] + fill[j]++] = i;
            }
        }
    }

    // Bucket-queue smallest-degree-last ordering
    int max_deg = *std::max_element(deg.begin(), deg.end());
    if (max_deg < 0) max_deg = 0;

    // Doubly-linked list nodes per vertex
    std::vector<int> nxt(n, -1);
    std::vector<int> prv(n, -1);
    std::vector<int> bucket_head(max_deg + 1, -1);
    std::vector<int> cur_deg(deg.begin(), deg.end());

    // Insert all vertices into their degree buckets
    for (int v = 0; v < n; ++v) {
        int d = cur_deg[v];
        nxt[v] = bucket_head[d];
        prv[v] = -1;
        if (bucket_head[d] != -1)
            prv[bucket_head[d]] = v;
        bucket_head[d] = v;
    }

    int min_bucket = 0;
    std::vector<int> order(n);
    std::vector<bool> removed(n, false);

    for (int step = 0; step < n; ++step) {
        // Advance min_bucket to first non-empty bucket
        while (min_bucket <= max_deg && bucket_head[min_bucket] == -1)
            ++min_bucket;

        int v = bucket_head[min_bucket];

        // Remove v from its bucket
        bucket_head[min_bucket] = nxt[v];
        if (nxt[v] != -1)
            prv[nxt[v]] = -1;

        removed[v] = true;
        order[step] = v;

        // Decrement neighbors' degrees and move them down one bucket
        for (int ap = adj_ptr[v]; ap < adj_ptr[v + 1]; ++ap) {
            int u = adj_list[ap];
            if (removed[u]) continue;

            int old_d = cur_deg[u];

            // Remove u from bucket old_d
            if (prv[u] == -1) {
                bucket_head[old_d] = nxt[u];
            } else {
                nxt[prv[u]] = nxt[u];
            }
            if (nxt[u] != -1) {
                prv[nxt[u]] = prv[u];
            }

            // Insert u into bucket old_d - 1
            int new_d = old_d - 1;
            cur_deg[u] = new_d;
            nxt[u] = bucket_head[new_d];
            prv[u] = -1;
            if (bucket_head[new_d] != -1)
                prv[bucket_head[new_d]] = u;
            bucket_head[new_d] = u;

            if (new_d < min_bucket)
                min_bucket = new_d;
        }
    }

    // Greedy coloring in reverse SDL order
    std::fill(colors_out, colors_out + n, -1);

    int num_colors_used = 0;
    std::vector<bool> used;

    for (int step = n - 1; step >= 0; --step) {
        int v = order[step];

        if ((int)used.size() < num_colors_used + 1)
            used.resize(num_colors_used + 1, false);

        // Mark neighbor colors as used
        int mark_count = 0;
        for (int ap = adj_ptr[v]; ap < adj_ptr[v + 1]; ++ap) {
            int u = adj_list[ap];
            int c = colors_out[u];
            if (c >= 0 && !used[c]) {
                used[c] = true;
                ++mark_count;
            }
        }

        // Find smallest unused color
        int c = 0;
        while (c < (int)used.size() && used[c])
            ++c;
        colors_out[v] = c;
        if (c >= num_colors_used) {
            num_colors_used = c + 1;
            used.resize(num_colors_used + 1, false);
        }

        // Clear used marks
        for (int ap = adj_ptr[v]; ap < adj_ptr[v + 1]; ++ap) {
            int u = adj_list[ap];
            int cu = colors_out[u];
            if (cu >= 0)
                used[cu] = false;
        }
    }
}
