#ifndef HAL_INTERFACE_H
#define HAL_INTERFACE_H
#include <stddef.h>
#include <stdint.h>
#include "tile_memory.h"
#include "c0_master/c0_controller.h"

typedef enum {
    MEM_DLM_64,
    MEM_DLM1_512,
    MEM_DMEM_512
} mem_type_t;

typedef struct {
    int (*cpu_local_move)(uint64_t src_addr, uint64_t dst_addr, size_t size);
    int (*dma_local_transfer)(int tile_id, uint64_t src_addr, uint64_t dst_addr, size_t size);
    int (*dma_remote_transfer)(uint64_t src_addr, uint64_t dst_addr, size_t size);
    int (*dmem_to_dmem_transfer)(uint64_t src_addr, uint64_t dst_addr, size_t size);
    int (*node_sync)(int node_mask);
    int (*get_dmem_status)(uint64_t dmem_base_addr);
    int (*mesh_route_optimal)(uint64_t src_addr, uint64_t dst_addr);
    // Memory access functions for test setup/verification
    int (*memory_read)(uint64_t addr, uint8_t* buffer, size_t size);
    int (*memory_write)(uint64_t addr, const uint8_t* buffer, size_t size);
    int (*memory_fill)(uint64_t addr, uint8_t value, size_t size);
    int (*memory_set)(uint64_t addr, uint8_t value, size_t size);
} hal_interface_t;

extern hal_interface_t g_hal;

void hal_use_reference_impl(void);

void hal_set_platform(mesh_platform_t* p);
#endif