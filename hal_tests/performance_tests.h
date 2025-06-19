#ifndef PERFORMANCE_TESTS_H
#define PERFORMANCE_TESTS_H
#include "c0_master/c0_controller.h"

int test_noc_bandwidth(mesh_platform_t* p);
int test_noc_latency(mesh_platform_t* p);

#endif
