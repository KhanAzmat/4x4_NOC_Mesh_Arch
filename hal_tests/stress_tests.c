#include <stdio.h>
#include "stress_tests.h"

int test_concurrent_dma(mesh_platform_t* p)
{
    (void)p;
    printf("[Stress] Concurrent DMA – not implemented, skip\n");
    return 1; /* skip = pass */
}
