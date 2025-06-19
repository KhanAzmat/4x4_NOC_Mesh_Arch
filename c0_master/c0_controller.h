#ifndef C0_CONTROLLER_H
#define C0_CONTROLLER_H

#include <stdint.h>

typedef struct {
    int id;
    int x, y;
    
    // Address space (what software sees)
    uint64_t dlm64_base_addr;
    uint64_t dlm1_512_base_addr;
    uint64_t dma_reg_base_addr;
    
    // Simulated hardware memory (what tests access for verification)
    uint8_t* dlm64_ptr;
    uint8_t* dlm1_512_ptr;
    uint8_t* dma_regs_ptr;
} tile_core_t;

typedef struct {
    int id;
    uint64_t dmem_base_addr;    // Address space
    uint8_t* dmem_ptr;          // Simulated memory
    size_t dmem_size;
} dmem_module_t;

typedef struct {
    tile_core_t* nodes;
    int node_count;
    dmem_module_t* dmems;
    int dmem_count;
    

    uint8_t* memory_pool;
    size_t total_memory_size;
} mesh_platform_t;


#endif