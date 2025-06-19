#include "address_manager.h"
#include "c0_master/c0_controller.h"
#include <stdlib.h>
#include <string.h>

static mesh_platform_t* g_platform = NULL;
static uint8_t* g_memory_pool = NULL;

void address_manager_init(void* platform) {
    g_platform = (mesh_platform_t*)platform;
    
    // Allocate memory pool to simulate address space
    size_t total_size = 0x30000000UL; // 768MB simulation space
    g_memory_pool = calloc(1, total_size);
    g_platform->memory_pool = g_memory_pool;
    g_platform->total_memory_size = total_size;
}

uint8_t* addr_to_ptr(uint64_t address) {
    if (!g_platform) return NULL;

    // Check if it's a tile DLM1_512 address
    for (int tile = 0; tile < g_platform->node_count; tile++) {
        uint64_t tile_base = g_platform->nodes[tile].dlm1_512_base_addr;
        if (address >= tile_base && address < tile_base + DLM1_512_SIZE) {
            uint64_t offset = address - tile_base;
            return g_platform->nodes[tile].dlm1_512_ptr + offset;
        }
    }
    
    // Check if it's a DMEM address
    for (int dmem = 0; dmem < g_platform->dmem_count; dmem++) {
        uint64_t dmem_base = g_platform->dmems[dmem].dmem_base_addr;
        if (address >= dmem_base && address < dmem_base + g_platform->dmems[dmem].dmem_size) {
            uint64_t offset = address - dmem_base;
            return g_platform->dmems[dmem].dmem_ptr + offset;
        }
    }
    
    // Fallback to memory pool for other regions (C0 control, etc.)
    if (g_memory_pool) {
        if (address >= C0_MASTER_BASE && address < C0_MASTER_BASE + C0_MASTER_SIZE) {
            uint64_t offset = 0x25000000UL;
            return g_memory_pool + offset + (address - C0_MASTER_BASE);
        }
    }
    
    return NULL; // Invalid address
}

uint64_t ptr_to_addr(void* ptr) {
    if (!g_memory_pool || !ptr) return 0;
    
    uint8_t* byte_ptr = (uint8_t*)ptr;
    ptrdiff_t offset = byte_ptr - g_memory_pool;
    
    // Reverse the mapping from addr_to_ptr
    if (offset >= 0x20000000UL && offset < 0x25000000UL) {
        // DMEM region
        return DMEM0_512_BASE + (offset - 0x20000000UL);
    }
    else if (offset >= 0x25000000UL && offset < 0x26000000UL) {
        // C0 control region
        return C0_MASTER_BASE + (offset - 0x25000000UL);
    }
    else if (offset >= 0 && offset < 0x20000000UL) {
        // Tile region
        return (uint64_t)offset;
    }
    
    return 0; // Invalid
}

int validate_address(uint64_t address, size_t size) {
    if (size == 0) return 0;
    
    addr_region_t region = get_address_region(address);
    if (region == ADDR_INVALID) return 0;
    
    // Check that the entire range is valid
    uint64_t end_addr = address + size - 1;
    addr_region_t end_region = get_address_region(end_addr);
    
    return (region == end_region) ? 1 : 0;
}

addr_region_t get_address_region(uint64_t address) {
    // Check if it's in any tile's address space
    for (int tile = 0; tile < NUM_TILES; tile++) {
        uint64_t tile_base = TILE0_BASE + (tile * TILE_STRIDE);
        
        if (address >= tile_base && address < tile_base + TILE_STRIDE) {
            uint64_t offset = address - tile_base;
            
            if (offset >= DLM_64_OFFSET && offset < DLM_64_OFFSET + DLM_64_SIZE) {
                return ADDR_TILE_DLM64;
            }
            else if (offset >= DLM1_512_OFFSET && offset < DLM1_512_OFFSET + DLM1_512_SIZE) {
                return ADDR_TILE_DLM1_512;
            }
            else if (offset >= DMA_REG_OFFSET && offset < DMA_REG_OFFSET + 0x1000) {
                return ADDR_TILE_DMA_REG;
            }
            
            return ADDR_INVALID; // Within tile space but unknown region
        }
    }
    
    // Check DMEM regions
    uint64_t dmem_bases[] = {
        DMEM0_512_BASE, DMEM1_512_BASE, DMEM2_512_BASE, DMEM3_512_BASE,
        DMEM4_512_BASE, DMEM5_512_BASE, DMEM6_512_BASE, DMEM7_512_BASE
    };
    
    for (int dmem = 0; dmem < NUM_DMEMS; dmem++) {
        if (address >= dmem_bases[dmem] && address < dmem_bases[dmem] + DMEM_512_SIZE) {
            return ADDR_DMEM_512;
        }
    }
    
    // Check C0 Master region
    if (address >= C0_MASTER_BASE && address < C0_MASTER_BASE + C0_MASTER_SIZE) {
        return ADDR_C0_MASTER;
    }
    
    return ADDR_INVALID;
}

int get_tile_id_from_address(uint64_t address) {
    for (int tile = 0; tile < NUM_TILES; tile++) {
        uint64_t tile_base = TILE0_BASE + (tile * TILE_STRIDE);
        if (address >= tile_base && address < tile_base + TILE_STRIDE) {
            return tile;
        }
    }
    return -1; // Invalid
}

int get_dmem_id_from_address(uint64_t address) {
    uint64_t dmem_bases[] = {
        DMEM0_512_BASE, DMEM1_512_BASE, DMEM2_512_BASE, DMEM3_512_BASE,
        DMEM4_512_BASE, DMEM5_512_BASE, DMEM6_512_BASE, DMEM7_512_BASE
    };
    
    for (int dmem = 0; dmem < NUM_DMEMS; dmem++) {
        if (address >= dmem_bases[dmem] && address < dmem_bases[dmem] + DMEM_512_SIZE) {
            return dmem;
        }
    }
    return -1; // Invalid
}

void register_memory_region(uint64_t addr, uint8_t* ptr, size_t size) {
    // For simulation, this function registers address->pointer mappings
    // The actual mapping is handled by addr_to_ptr() function
    // In a real implementation, this would set up memory management tables
    
    // For now, this is a placeholder since addr_to_ptr() already handles
    // the address translation based on the platform structure
    (void)addr; (void)ptr; (void)size; // Suppress unused parameter warnings
}