#pragma once

// CSC sparse matrix view
// indptr[j] ... indptr[j+1] gives the row indices for column j.
struct CSCMatrix {
    const int* indptr;   // length num_cols + 1
    const int* indices;  // length nnz
    const int* data;     // length nnz
    int num_cols;
};
