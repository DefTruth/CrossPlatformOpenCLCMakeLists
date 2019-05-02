kernel void vectorAdd (global const int* restrict a, global const int* restrict b, global int* restrict c)
{
    // Get the global index of the Work-Item to be processed
    int i = get_global_id (0);

    // Do the operation on the corresponding buffer field
    c[i] = a[i] + b[i];
}

kernel void vectorAddSingleWorkItem (global const int* restrict a, global const int* restrict b, global int* restrict c, const int count)
{
    for (int i = 0; i < count; ++i)
        c[i] = a[i] + b[i];
}