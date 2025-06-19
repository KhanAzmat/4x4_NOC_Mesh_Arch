#ifndef BASIC_TESTS_H
#define BASIC_TESTS_H
#include "c0_master/c0_controller.h"

int test_cpu_local_move(mesh_platform_t* p);
int test_dma_local_transfer(mesh_platform_t* p);
int test_dma_remote_transfer(mesh_platform_t* p);

#endif
