#include "c0_master/c0_controller.h"  // Must be first to get mesh_platform_t definition
#include "dmac512_hardware_monitor.h"
#include "address_manager.h"
#include "hal_dmac512.h"
#include <stdio.h>
#include <string.h>
#include <stddef.h>  // For offsetof

// Each tile monitors its own DMA registers for hardware emulation
// No HAL modification required - tiles act as DMA hardware controllers

/**
 * @brief Check if DMA hardware should execute a transfer for this tile
 * 
 * This function is called by each tile thread to emulate DMA hardware behavior.
 * When the HAL writes to DMA registers and sets enable=1, this detects it
 * and performs the actual memory transfer like real DMA hardware would.
 */
int dmac512_monitor_tile_registers(int tile_id, mesh_platform_t* platform) {
    if (tile_id < 0 || tile_id >= NUM_TILES || !platform) {
        return 0;
    }
    
    tile_core_t* tile = &platform->nodes[tile_id];
    if (!tile || !tile->dmac512_regs) {
        return 0;
    }
    
    DMAC512_RegDef* regs = tile->dmac512_regs;
    
    // Check if DMA is enabled (HAL has written enable bit = 1 in DMAC_TOTAL_XFER_CNT[31])
    uint32_t xfer_cnt_reg = regs->DMAC_TOTAL_XFER_CNT;
    bool dma_enabled = (xfer_cnt_reg & DMAC512_TOTAL_XFER_CNT_DMAC_EN_MASK) != 0;
    
    if (!dma_enabled) {
        return 0; // No transfer pending
    }
    
    // DMA transfer is requested by HAL - act as hardware controller
    printf("[DMAC512-HW] Tile %d: DMA enabled, executing transfer as hardware\n", tile_id);
    
    // Read transfer parameters from registers (written by HAL)
    uint64_t src_addr = regs->DMAC_SRC_ADDR;
    uint64_t dst_addr = regs->DMAC_DST_ADDR;
    uint32_t transfer_size = xfer_cnt_reg & DMAC512_TOTAL_XFER_CNT_MASK;
    
    printf("[DMAC512-HW] Tile %d: Transfer 0x%lx -> 0x%lx (size=%u)\n", 
           tile_id, src_addr, dst_addr, transfer_size);
    
    // Perform actual memory copy (hardware behavior)
    int copy_result = 0;
    if (transfer_size > 0 && transfer_size <= (1024 * 1024)) { // Safety limit
        // Use platform's address manager to do the copy
        copy_result = platform_memory_copy(platform, src_addr, dst_addr, transfer_size);
        
        if (copy_result == 0) {
            printf("[DMAC512-HW] Tile %d: Memory copy completed successfully\n", tile_id);
        } else {
            printf("[DMAC512-HW] Tile %d: Memory copy failed with error %d\n", tile_id, copy_result);
        }
    } else {
        printf("[DMAC512-HW] Tile %d: Invalid transfer size %u, skipping\n", tile_id, transfer_size);
        copy_result = -1;
    }
    
    // Hardware completion: Clear enable bit (like real DMA hardware)
    regs->DMAC_TOTAL_XFER_CNT &= ~DMAC512_TOTAL_XFER_CNT_DMAC_EN_MASK;
    
    // Generate completion interrupt (like real DMA hardware)
    // Note: We need to forward declare this function to avoid include cycle
    extern void device_dma_completion_interrupt(uint32_t source_hart_id, uint32_t transfer_id);
    device_dma_completion_interrupt(tile_id, transfer_size);
    
    printf("[DMAC512-HW] Tile %d: DMA transfer completed, enable bit cleared\n", tile_id);
    
    return (copy_result == 0) ? 1 : 0; // Return 1 if transfer was executed
}

/**
 * @brief Perform platform memory copy using address manager
 */
int platform_memory_copy(mesh_platform_t* platform, uint64_t src_addr, uint64_t dst_addr, size_t size) {
    if (!platform || size == 0) {
        return -1;
    }
    
    // Use address manager to translate addresses to simulation pointers
    void* src_ptr = addr_to_ptr(src_addr);
    void* dst_ptr = addr_to_ptr(dst_addr);
    
    if (!src_ptr || !dst_ptr) {
        printf("[DMAC512-HW] Address translation failed: src=0x%lx->%p, dst=0x%lx->%p\n",
               src_addr, src_ptr, dst_addr, dst_ptr);
        return -2;
    }
    
    // Perform memory copy
    memcpy(dst_ptr, src_ptr, size);
    
    return 0;
}

/**
 * @brief Synchronous DMA execution hook - called immediately when enable bit is written
 */
int dmac512_execute_on_enable_write(int tile_id, mesh_platform_t* platform, DMAC512_RegDef* regs) {
    if (tile_id < 0 || tile_id >= NUM_TILES || !platform || !regs) {
        return 0;
    }
    
    // Check if DMA is enabled (HAL just wrote enable bit = 1)
    uint32_t xfer_cnt_reg = regs->DMAC_TOTAL_XFER_CNT;
    bool dma_enabled = (xfer_cnt_reg & DMAC512_TOTAL_XFER_CNT_DMAC_EN_MASK) != 0;
    
    if (!dma_enabled) {
        return 0; // Enable bit not set, nothing to do
    }
    
    // DMA transfer is requested by HAL - execute immediately (synchronous hardware behavior)
    printf("[DMAC512-HW-SYNC] Tile %d: DMA enable detected, executing transfer synchronously\n", tile_id);
    
    // Read transfer parameters from registers (written by HAL)
    uint64_t src_addr = regs->DMAC_SRC_ADDR;
    uint64_t dst_addr = regs->DMAC_DST_ADDR;
    uint32_t transfer_size = xfer_cnt_reg & DMAC512_TOTAL_XFER_CNT_MASK;
    
    printf("[DMAC512-HW-SYNC] Tile %d: Transfer 0x%lx -> 0x%lx (size=%u)\n", 
           tile_id, src_addr, dst_addr, transfer_size);
    
    // Perform actual memory copy (hardware behavior) - IMMEDIATELY
    int copy_result = 0;
    if (transfer_size > 0 && transfer_size <= (1024 * 1024)) { // Safety limit
        // Use platform's address manager to do the copy
        copy_result = platform_memory_copy(platform, src_addr, dst_addr, transfer_size);
        
        if (copy_result == 0) {
            printf("[DMAC512-HW-SYNC] Tile %d: Memory copy completed successfully\n", tile_id);
        } else {
            printf("[DMAC512-HW-SYNC] Tile %d: Memory copy failed with error %d\n", tile_id, copy_result);
        }
    } else {
        printf("[DMAC512-HW-SYNC] Tile %d: Invalid transfer size %u, skipping\n", tile_id, transfer_size);
        copy_result = -1;
    }
    
    // Hardware completion: Clear enable bit (like real DMA hardware)
    regs->DMAC_TOTAL_XFER_CNT &= ~DMAC512_TOTAL_XFER_CNT_DMAC_EN_MASK;
    
    // Generate completion interrupt (like real DMA hardware)
    extern void device_dma_completion_interrupt(uint32_t source_hart_id, uint32_t transfer_id);
    device_dma_completion_interrupt(tile_id, transfer_size);
    
    printf("[DMAC512-HW-SYNC] Tile %d: DMA transfer completed synchronously, enable bit cleared\n", tile_id);
    
    return (copy_result == 0) ? 1 : 0; // Return 1 if transfer was executed
}

/**
 * @brief Hook function called whenever DMAC_TOTAL_XFER_CNT register is written
 */
void dmac512_on_register_write_hook(int tile_id, mesh_platform_t* platform, 
                                   uint32_t reg_offset, uint32_t new_value) {
    if (!platform || tile_id < 0 || tile_id >= NUM_TILES) {
        return;
    }
    
    // Only process writes to DMAC_TOTAL_XFER_CNT register (contains enable bit)
    if (reg_offset != offsetof(DMAC512_RegDef, DMAC_TOTAL_XFER_CNT)) {
        return;
    }
    
    // Check if enable bit was just set
    bool enable_bit_set = (new_value & DMAC512_TOTAL_XFER_CNT_DMAC_EN_MASK) != 0;
    if (!enable_bit_set) {
        return; // Enable bit not set, no transfer to execute
    }
    
    printf("[DMAC512-HOOK] Tile %d: DMAC_TOTAL_XFER_CNT write detected with enable=1\n", tile_id);
    
    // Get tile's DMA registers
    tile_core_t* tile = &platform->nodes[tile_id];
    if (!tile || !tile->dmac512_regs) {
        printf("[DMAC512-HOOK] Tile %d: Invalid tile or DMA registers\n", tile_id);
        return;
    }
    
    // Execute DMA transfer immediately (synchronous with HAL write)
    dmac512_execute_on_enable_write(tile_id, platform, tile->dmac512_regs);
}

/**
 * @brief Platform function to enable DMA with immediate execution
 */
void platform_dmac512_enable_and_execute(DMAC512_RegDef* regs, int enable_value) {
    if (!regs) {
        return;
    }
    
    // Get global platform (must be set during initialization)
    extern mesh_platform_t* global_platform_ptr;
    if (!global_platform_ptr) {
        printf("[DMAC512-PLATFORM] Error: global_platform_ptr not set\n");
        return;
    }
    
    // Find which tile owns these registers
    int tile_id = platform_get_tile_id_from_dmac_regs(regs);
    if (tile_id < 0) {
        printf("[DMAC512-PLATFORM] Error: Could not find tile for register %p\n", regs);
        return;
    }
    
    // Perform the register write (original HAL operation)
    regs->DMAC_TOTAL_XFER_CNT = ((regs->DMAC_TOTAL_XFER_CNT) & ~DMAC512_TOTAL_XFER_CNT_DMAC_EN_MASK) | 
                                ((enable_value) << DMAC512_TOTAL_XFER_CNT_DMAC_EN_SHIFT);
    
    printf("[DMAC512-PLATFORM] Tile %d: DMA enable=%d written to register\n", tile_id, enable_value);
    
    // If enabling, execute transfer immediately (synchronous hardware behavior)
    if (enable_value) {
        printf("[DMAC512-PLATFORM] Tile %d: Executing DMA transfer synchronously\n", tile_id);
        dmac512_execute_on_enable_write(tile_id, global_platform_ptr, regs);
    }
}

/**
 * @brief Get tile ID from DMAC register pointer
 */
int platform_get_tile_id_from_dmac_regs(DMAC512_RegDef* regs) {
    extern mesh_platform_t* global_platform_ptr;
    if (!global_platform_ptr || !regs) {
        return -1;
    }
    
    // Check each tile's DMA register address
    for (int i = 0; i < NUM_TILES; i++) {
        if (global_platform_ptr->nodes[i].dmac512_regs == regs) {
            return i;
        }
    }
    
    return -1; // Register not found in any tile
} 