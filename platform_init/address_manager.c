#include "address_manager.h"
#include "c0_master/c0_controller.h"
#include "hal_dmac512.h"   // For DMAC512 register definitions
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

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

    // Check if it's a tile DLM_64 address
    for (int tile = 0; tile < g_platform->node_count; tile++) {
        uint64_t dlm64_base = g_platform->nodes[tile].dlm64_base_addr;
        if (address >= dlm64_base && address < dlm64_base + DLM_64_SIZE) {
            uint64_t offset = address - dlm64_base;
            return g_platform->nodes[tile].dlm64_ptr + offset;
        }
    }

    // Check if it's a tile DLM1_512 address
    for (int tile = 0; tile < g_platform->node_count; tile++) {
        uint64_t tile_base = g_platform->nodes[tile].dlm1_512_base_addr;
        if (address >= tile_base && address < tile_base + DLM1_512_SIZE) {
            uint64_t offset = address - tile_base;
            return g_platform->nodes[tile].dlm1_512_ptr + offset;
        }
    }
    
    // Check if it's a tile DMA register address
    for (int tile = 0; tile < g_platform->node_count; tile++) {
        uint64_t dma_base = g_platform->nodes[tile].dma_reg_base_addr;
        if (address >= dma_base && address < dma_base + 0x1000) {
            uint64_t offset = address - dma_base;
            return g_platform->nodes[tile].dma_regs_ptr + offset;
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

// ====== DMAC512 Phase 2 Implementation ======

int dmac512_register_write_hook(uint64_t address, uint32_t value, size_t size) {
    if (!g_platform) return 0;
    
    // Check if this is a DMA register write
    addr_region_t region = get_address_region(address);
    if (region != ADDR_TILE_DMA_REG) {
        return 0; // Not a DMA register
    }
    
    // Get which tile this DMA register belongs to
    int tile_id = get_tile_id_from_address(address);
    if (tile_id < 0 || tile_id >= NUM_TILES) {
        return 0; // Invalid tile
    }
    
    // Calculate register offset within DMA register block
    uint64_t tile_base = TILE0_BASE + (tile_id * TILE_STRIDE);
    uint64_t dma_reg_base = tile_base + DMA_REG_OFFSET;
    uint64_t reg_offset = address - dma_reg_base;
    
    // Get the DMAC512 register structure for this tile
    DMAC512_RegDef* dmac_regs = g_platform->nodes[tile_id].dmac512_regs;
    if (!dmac_regs) {
        return 0; // DMAC512 not initialized
    }
    
    printf("[DMAC512] Tile %d: Register write at offset 0x%lX = 0x%X (size %zu)\n", 
           tile_id, reg_offset, value, size);
    
    // Handle specific register writes
    switch (reg_offset) {
        case 0x00: // DMAC_CONTROL register
            dmac_regs->DMAC_CONTROL = value;
            break;
            
        case 0x04: // DMAC_STATUS register (usually read-only, but allow writes for simulation)
            dmac_regs->DMAC_STATUS = value;
            break;
            
        case 0x10: // DMAC_INTR register
            dmac_regs->DMAC_INTR = value;
            break;
            
        case 0x20: // DMAC_SRC_ADDR (64-bit, low 32 bits)
            dmac_regs->DMAC_SRC_ADDR = (dmac_regs->DMAC_SRC_ADDR & 0xFFFFFFFF00000000ULL) | value;
            break;
            
        case 0x24: // DMAC_SRC_ADDR (64-bit, high 32 bits)
            dmac_regs->DMAC_SRC_ADDR = (dmac_regs->DMAC_SRC_ADDR & 0x00000000FFFFFFFFULL) | ((uint64_t)value << 32);
            break;
            
        case 0x30: // DMAC_DST_ADDR (64-bit, low 32 bits)
            dmac_regs->DMAC_DST_ADDR = (dmac_regs->DMAC_DST_ADDR & 0xFFFFFFFF00000000ULL) | value;
            break;
            
        case 0x34: // DMAC_DST_ADDR (64-bit, high 32 bits)
            dmac_regs->DMAC_DST_ADDR = (dmac_regs->DMAC_DST_ADDR & 0x00000000FFFFFFFFULL) | ((uint64_t)value << 32);
            break;
            
        case 0x40: // DMAC_TOTAL_XFER_CNT
            {
                uint32_t old_xfer_cnt = dmac_regs->DMAC_TOTAL_XFER_CNT;
                dmac_regs->DMAC_TOTAL_XFER_CNT = value;
                
                // Check if DMAC enable bit was set (bit 31)
                bool was_enabled = (old_xfer_cnt & DMAC512_TOTAL_XFER_CNT_DMAC_EN_MASK) != 0;
                bool now_enabled = (value & DMAC512_TOTAL_XFER_CNT_DMAC_EN_MASK) != 0;
                
                if (!was_enabled && now_enabled) {
                    printf("[DMAC512] Tile %d: DMA enabled, executing transfer...\n", tile_id);
                    dmac512_execute_transfer(tile_id);
                }
            }
            break;
            
        default:
            // Other registers - just store the value
            if (reg_offset < 0x1000 && (reg_offset % 4) == 0) {
                uint32_t* reg_ptr = (uint32_t*)((uint8_t*)dmac_regs + reg_offset);
                *reg_ptr = value;
            }
            break;
    }
    
    return 1; // Handled as DMAC512 register
}

int dmac512_execute_transfer(int tile_id) {
    if (!g_platform || tile_id < 0 || tile_id >= NUM_TILES) {
        return -1;
    }
    
    DMAC512_RegDef* dmac_regs = g_platform->nodes[tile_id].dmac512_regs;
    if (!dmac_regs) {
        return -1;
    }
    
    // Mark DMA as busy
    dmac_regs->DMAC_STATUS |= DMAC512_STATUS_DMAC_BUSY_MASK;
    g_platform->nodes[tile_id].dmac512_busy = true;
    
    // Get transfer parameters from registers
    uint64_t src_addr = dmac_regs->DMAC_SRC_ADDR;
    uint64_t dst_addr = dmac_regs->DMAC_DST_ADDR;
    uint32_t transfer_count = dmac_regs->DMAC_TOTAL_XFER_CNT & DMAC512_TOTAL_XFER_CNT_MASK;
    
    printf("[DMAC512] Tile %d: Executing transfer\n", tile_id);
    printf("  Source: 0x%016lX\n", src_addr);
    printf("  Dest:   0x%016lX\n", dst_addr);
    printf("  Count:  %u bytes\n", transfer_count);
    
    // Validate addresses and perform transfer
    uint8_t* src_ptr = addr_to_ptr(src_addr);
    uint8_t* dst_ptr = addr_to_ptr(dst_addr);
    
    if (!src_ptr || !dst_ptr || transfer_count == 0) {
        printf("[DMAC512] Tile %d: Transfer failed - invalid addresses or count\n", tile_id);
        dmac_regs->DMAC_STATUS &= ~DMAC512_STATUS_DMAC_BUSY_MASK;
        g_platform->nodes[tile_id].dmac512_busy = false;
        return -1;
    }
    
    // Perform the memory copy
    memcpy(dst_ptr, src_ptr, transfer_count);
    printf("[DMAC512] Tile %d: Transfer completed successfully\n", tile_id);
    
    // Mark transfer as complete
    dmac_regs->DMAC_STATUS &= ~DMAC512_STATUS_DMAC_BUSY_MASK;
    g_platform->nodes[tile_id].dmac512_busy = false;
    
    // Set completion interrupt if enabled (check interrupt mask register)
    if (!(dmac_regs->DMAC_INTR_MASK & DMAC512_INTR_DMAC_INTR_MASK)) {
        dmac_regs->DMAC_INTR |= DMAC512_INTR_DMAC_INTR_MASK;
        printf("[DMAC512] Tile %d: Completion interrupt set\n", tile_id);
    }
    
    // Clear enable bit to prevent repeated execution
    dmac_regs->DMAC_TOTAL_XFER_CNT &= ~DMAC512_TOTAL_XFER_CNT_DMAC_EN_MASK;
    
    return 0;
}