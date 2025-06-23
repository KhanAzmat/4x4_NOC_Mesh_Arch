#include <stdio.h>
#include "test_framework.h"
#include "basic_tests.h"
#include "performance_tests.h"
#include "c0_tests.h"
#include "stress_tests.h"
#include "random_dma_tests.h"

void run_all_tests(mesh_platform_t* platform)
{
    int passed = 0, total = 0;
    total++; passed += test_cpu_local_move(platform);   
    total++; passed += test_dma_local_transfer(platform);
    total++; passed += test_dma_remote_transfer(platform);
    total++; passed += test_c0_gather(platform);
    total++; passed += test_c0_distribute(platform);
    total++; passed += test_noc_bandwidth(platform);
    total++; passed += test_noc_latency(platform);
    total++; passed += test_random_dma_remote(platform);
printf("\n\033[1mSummary: %d/%d tests passed\033[0m\n", passed, total);
}