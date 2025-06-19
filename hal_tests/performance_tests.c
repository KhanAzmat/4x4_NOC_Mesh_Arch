#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "performance_tests.h"
#include "hal_tests/hal_interface.h"

int test_noc_bandwidth(mesh_platform_t* p)
{
    const size_t bytes = 64 * 1024; // Use smaller size that fits in DLM1_512
    
    // Use platform addresses instead of malloc
    uint64_t src_addr = TILE0_DLM1_512_BASE;
    uint64_t dst_addr = TILE0_DLM1_512_BASE + bytes;
    
    clock_t t0 = clock();
    g_hal.cpu_local_move(src_addr, dst_addr, bytes);
    clock_t t1 = clock();
    double secs = (double)(t1 - t0) / CLOCKS_PER_SEC;
    double bw = bytes / (1024.0*1024.0) / secs;
    printf("[Perf] CPU local move bandwidth: %.2f MB/s\n", bw);
    
    return 1; /* always pass */
}

int test_noc_latency(mesh_platform_t* p)
{
    const size_t bytes = 64;
    
    // Use remote transfer to measure NoC latency 
    uint64_t src_addr = TILE0_DLM1_512_BASE;
    uint64_t dst_addr = DMEM0_512_BASE;
    
    clock_t t0 = clock();
    g_hal.dma_remote_transfer(src_addr, dst_addr, bytes);
    clock_t t1 = clock();
    double ns = (double)(t1 - t0) * 1e9 / CLOCKS_PER_SEC;
    printf("[Perf] NoC latency (DMA remote): %.0f ns\n", ns);
    printf("\n");
    return 1;
}
