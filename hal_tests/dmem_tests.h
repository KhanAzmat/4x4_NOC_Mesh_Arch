// ===================== File: hal_tests/dmem_tests.h =====================
#ifndef DMEM_TESTS_H
#define DMEM_TESTS_H

#include "c0_master/c0_controller.h"

int test_dmem_basic_functionality(mesh_platform_t* p);
int test_dmem_large_transfers(mesh_platform_t* p);
int test_dmem_address_validation(mesh_platform_t* p);
int test_dmem_data_integrity(mesh_platform_t* p);
int test_dmem_concurrent_access(mesh_platform_t* p);
int test_dmem_boundary_conditions(mesh_platform_t* p);
int test_dmem_error_handling(mesh_platform_t* p);
int test_dmem_performance_basic(mesh_platform_t* p);
int test_dmem_cross_module_transfers(mesh_platform_t* p);
int test_dmem_alignment_testing(mesh_platform_t* p);

#endif // DMEM_TESTS_H
