#ifndef DMAC512_COMPREHENSIVE_TESTS_H
#define DMAC512_COMPREHENSIVE_TESTS_H

#include "c0_master/c0_controller.h"

// Main test suite function
int run_dmac512_comprehensive_tests(mesh_platform_t* platform);

// Handle Management Tests (6 tests)
int test_dmac512_handle_init_valid(mesh_platform_t* p);
int test_dmac512_handle_init_null_pointer(mesh_platform_t* p);
int test_dmac512_handle_init_invalid_address(mesh_platform_t* p);
int test_dmac512_handle_multiple_tiles(mesh_platform_t* p);
int test_dmac512_handle_reinitialization(mesh_platform_t* p);
int test_dmac512_handle_concurrent_access(mesh_platform_t* p);

// Configuration Tests (8 tests)
int test_dmac512_config_basic_transfer(mesh_platform_t* p);
int test_dmac512_config_different_beat_modes(mesh_platform_t* p);
int test_dmac512_config_normal_mode(mesh_platform_t* p);
int test_dmac512_config_zero_transfer_count(mesh_platform_t* p);
int test_dmac512_config_max_transfer_count(mesh_platform_t* p);
int test_dmac512_config_null_handle(mesh_platform_t* p);
int test_dmac512_config_sequential_configs(mesh_platform_t* p);
int test_dmac512_config_parameter_validation(mesh_platform_t* p);

// Transfer Control Tests (8 tests)
int test_dmac512_start_basic_transfer(mesh_platform_t* p);
int test_dmac512_start_without_config(mesh_platform_t* p);
int test_dmac512_start_multiple_consecutive(mesh_platform_t* p);
int test_dmac512_start_overlapping_transfers(mesh_platform_t* p);
int test_dmac512_start_all_tiles_parallel(mesh_platform_t* p);
int test_dmac512_start_null_handle(mesh_platform_t* p);
int test_dmac512_start_enable_bit_verification(mesh_platform_t* p);
int test_dmac512_start_interrupt_generation(mesh_platform_t* p);

// Status Monitoring Tests (6 tests)
int test_dmac512_busy_status_idle(mesh_platform_t* p);
int test_dmac512_busy_status_during_transfer(mesh_platform_t* p);
int test_dmac512_busy_status_after_completion(mesh_platform_t* p);
int test_dmac512_busy_status_null_handle(mesh_platform_t* p);
int test_dmac512_busy_status_multiple_tiles(mesh_platform_t* p);
int test_dmac512_busy_status_timeout_handling(mesh_platform_t* p);

// Register Access Tests (10 tests)
int test_dmac512_control_register_access(mesh_platform_t* p);
int test_dmac512_status_register_access(mesh_platform_t* p);
int test_dmac512_interrupt_register_access(mesh_platform_t* p);
int test_dmac512_src_addr_register_access(mesh_platform_t* p);
int test_dmac512_dst_addr_register_access(mesh_platform_t* p);
int test_dmac512_xfer_count_register_access(mesh_platform_t* p);
int test_dmac512_register_bit_field_verification(mesh_platform_t* p);
int test_dmac512_register_write_read_consistency(mesh_platform_t* p);
int test_dmac512_register_reset_values(mesh_platform_t* p);
int test_dmac512_register_concurrent_access(mesh_platform_t* p);

// Transfer Pattern Tests (6 tests)
int test_dmac512_tile_to_tile_transfer(mesh_platform_t* p);
int test_dmac512_tile_to_dmem_transfer(mesh_platform_t* p);
int test_dmac512_dmem_to_tile_transfer(mesh_platform_t* p);
int test_dmac512_different_sizes_transfer(mesh_platform_t* p);
int test_dmac512_address_alignment_transfer(mesh_platform_t* p);
int test_dmac512_cross_mesh_transfer(mesh_platform_t* p);

#endif // DMAC512_COMPREHENSIVE_TESTS_H 