#ifndef PLIC_COMPREHENSIVE_TESTS_H
#define PLIC_COMPREHENSIVE_TESTS_H

#include <stdint.h>
#include <stdbool.h>
#include "c0_master/c0_controller.h"
#include "hal/INT/plic.h"

// ============================================================================
// PLIC COMPREHENSIVE TEST SUITE
// ============================================================================
// These tests exercise the original PLIC HAL/Driver without any bypassing
// or placeholders. All tests call genuine HAL functions and verify actual
// register state and behavior.

// PLIC Instance Management Tests (6 tests)
int test_plic_initialization_valid_hart(mesh_platform_t* p);
int test_plic_initialization_invalid_hart(mesh_platform_t* p);
int test_plic_multiple_hart_initialization(mesh_platform_t* p);
int test_plic_instance_selection(mesh_platform_t* p);
int test_plic_hart_to_plic_mapping(mesh_platform_t* p);
int test_plic_concurrent_access(mesh_platform_t* p);

// PLIC Register Access Tests (8 tests)
int test_plic_version_and_capabilities(mesh_platform_t* p);
int test_plic_priority_set_get(mesh_platform_t* p);
int test_plic_priority_boundary_values(mesh_platform_t* p);
int test_plic_feature_enable_disable(mesh_platform_t* p);
int test_plic_pending_register_access(mesh_platform_t* p);
int test_plic_trigger_type_configuration(mesh_platform_t* p);
int test_plic_threshold_configuration(mesh_platform_t* p);
int test_plic_register_memory_mapping(mesh_platform_t* p);

// PLIC Interrupt Configuration Tests (8 tests)
int test_plic_enable_disable_interrupts(mesh_platform_t* p);
int test_plic_interrupt_priority_levels(mesh_platform_t* p);
int test_plic_multiple_interrupt_sources(mesh_platform_t* p);
int test_plic_target_enable_matrix(mesh_platform_t* p);
int test_plic_interrupt_source_validation(mesh_platform_t* p);
int test_plic_priority_threshold_filtering(mesh_platform_t* p);
int test_plic_interrupt_masking(mesh_platform_t* p);
int test_plic_cross_hart_interrupt_config(mesh_platform_t* p);

// PLIC Interrupt Flow Tests (6 tests)
int test_plic_basic_interrupt_flow(mesh_platform_t* p);
int test_plic_claim_complete_cycle(mesh_platform_t* p);
int test_plic_multiple_pending_interrupts(mesh_platform_t* p);
int test_plic_priority_based_delivery(mesh_platform_t* p);
int test_plic_interrupt_preemption(mesh_platform_t* p);
int test_plic_concurrent_interrupt_handling(mesh_platform_t* p);

// PLIC Error Handling Tests (4 tests)
int test_plic_invalid_source_ids(mesh_platform_t* p);
int test_plic_invalid_target_ids(mesh_platform_t* p);
int test_plic_null_pointer_handling(mesh_platform_t* p);
int test_plic_boundary_condition_handling(mesh_platform_t* p);

// Main test runner
int run_plic_comprehensive_tests(mesh_platform_t* platform);

// Test result statistics
typedef struct {
    int total_tests;
    int passed_tests;
    int failed_tests;
    uint64_t total_execution_time_us;
} plic_test_results_t;

extern plic_test_results_t g_plic_test_results;

#endif // PLIC_COMPREHENSIVE_TESTS_H 