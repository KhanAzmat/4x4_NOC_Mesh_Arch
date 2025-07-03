#ifndef ADDRESS_MANAGER_H
#define ADDRESS_MANAGER_H

#include <stdint.h>
#include <stddef.h>
#include "generated/mem_map.h"

// to simulate hardware addressing
uint8_t* addr_to_ptr(uint64_t address);
uint64_t ptr_to_addr(void* ptr);
int validate_address(uint64_t address, size_t size);

// Memory region identification
typedef enum {
    ADDR_TILE_DLM64,
    ADDR_TILE_DLM1_512, 
    ADDR_TILE_DMA_REG,
    ADDR_DMEM_512,
    ADDR_C0_MASTER,
    ADDR_INVALID
} addr_region_t;

// typedef struct mesh_platform_t mesh_platform_t;

addr_region_t get_address_region(uint64_t address);
int get_tile_id_from_address(uint64_t address);
int get_dmem_id_from_address(uint64_t address);

// Initialize the address manager
void address_manager_init(void* platform);

void register_memory_region(uint64_t addr, uint8_t* ptr, size_t size);

/**
 * @brief Register write hook for DMAC512 simulation
 * Called when any write occurs to DMA register addresses
 * @param address Register address being written to
 * @param value Value being written (32-bit)
 * @param size Write size in bytes
 * @return 1 if handled as DMAC512 register, 0 otherwise
 */
int dmac512_register_write_hook(uint64_t address, uint32_t value, size_t size);

/**
 * @brief Execute DMAC512 transfer for a specific tile
 * Called when enable bit is written to DMAC_CONTROL register
 * @param tile_id Tile ID (0-7)
 * @return 0 on success, -1 on error
 */
int dmac512_execute_transfer(int tile_id);

#endif