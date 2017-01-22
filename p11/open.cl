kernel void k_sin(global float* input, global float* output) {
    size_t i = get_global_id(0);
    output[i] = sin(input[i]);
}
