#include <stdio.h>
#include <stdlib.h>
#include <OpenCL/opencl.h>
#include "open.cl.h"
#include <math.h>

#define NUM_VALUES 102400

int main (int argc, const char * argv[]) {
    dispatch_queue_t queue = gcl_create_dispatch_queue(CL_DEVICE_TYPE_GPU, NULL);
    
    float* test_in = (float*)malloc(sizeof(cl_float) * NUM_VALUES);
    for (int i = 0; i < NUM_VALUES; i++) {
        test_in[i] = (cl_float)i/NUM_VALUES*100*M_PI;
    }
    
    float* test_out = (float*)malloc(sizeof(cl_float) * NUM_VALUES);
    
    void* mem_in  = gcl_malloc(sizeof(cl_float) * NUM_VALUES, test_in,
                               CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR);
    
    void* mem_out = gcl_malloc(sizeof(cl_float) * NUM_VALUES, NULL,
                               CL_MEM_WRITE_ONLY);
    
    dispatch_sync(queue, ^{
        size_t wgs;
        gcl_get_kernel_block_workgroup_info(k_sin_kernel,
                                            CL_KERNEL_WORK_GROUP_SIZE,
                                            sizeof(wgs), &wgs, NULL);
        
        cl_ndrange range = {1, {0, 0, 0}, {NUM_VALUES, 0, 0}, {wgs, 0, 0}};
        k_sin_kernel(&range,(cl_float*)mem_in, (cl_float*)mem_out);
        gcl_memcpy(test_out, mem_out, sizeof(cl_float) * NUM_VALUES);
    });
    
    for (int i = 0; i < NUM_VALUES; i++) {
        fprintf(stdout, "In: %1.4f Pi\tOut: %1.4f\n", test_in[i]/M_PI, test_out[i]);
        fflush(stdout);
    }

    gcl_free(mem_in);
    gcl_free(mem_out);
    
    free(test_in);
    free(test_out);
    
    dispatch_release(queue);
}
