#include <string.h>
#include "tile_dma.h"
#include "platform_init/address_manager.h"
#include "c0_master/c0_controller.h"

// Global handles for each tile's DMAC512 instance
static DMAC512_HandleTypeDef g_dmac512_handles[8];
static bool g_dmac512_initialized[8] = {false};

// External access to platform for getting tile handles
extern mesh_platform_t* g_platform;

/**
 * @brief Initialize DMAC512 for a specific tile
 */
int dma_tile_init(int tile_id) 
{
    if (tile_id < 0 || tile_id >= 8) {
        return -1;
    }
    
    if (g_dmac512_initialized[tile_id]) {
        return 0; // Already initialized
    }
    
    // Initialize DMAC512 handle for this tile
    int result = HAL_DMAC512InitTile(&g_dmac512_handles[tile_id], tile_id);
    if (result == 0) {
        g_dmac512_initialized[tile_id] = true;
    }
    
    return result;
}

/**
 * @brief Get DMAC512 handle for a tile
 */
int dma_tile_get_handle(int tile_id, DMAC512_HandleTypeDef** handle)
{
    if (tile_id < 0 || tile_id >= 8 || !handle) {
        return -1;
    }
    
    if (!g_dmac512_initialized[tile_id]) {
        int init_result = dma_tile_init(tile_id);
        if (init_result != 0) {
            return -1;
        }
    }
    
    *handle = &g_dmac512_handles[tile_id];
    return 0;
}

/**
 * @brief Updated DMA local transfer using DMAC512
 */
int dma_local_transfer(int tile_id, uint64_t src_addr, uint64_t dst_addr, size_t size)
{
    // Driver validates addresses are within the same tile
    if(get_tile_id_from_address(src_addr) != tile_id || 
       get_tile_id_from_address(dst_addr) != tile_id){
        return -1;
    }

    if(!validate_address(src_addr, size) || !validate_address(dst_addr, size)) {
        return -1;
    }

    // Get DMAC512 handle for this tile
    DMAC512_HandleTypeDef* dmac_handle;
    if (dma_tile_get_handle(tile_id, &dmac_handle) != 0) {
        return -1;
    }
    
    // Use DMAC512 for transfer
    return HAL_DMAC512Transfer(dmac_handle, src_addr, dst_addr, size);
}

// Legacy functions for backward compatibility
void dma_memcpy(void* dst, const void* src, size_t size)
{
    /* blocking copy for simulation */
    memcpy(dst, src, size);
}

void dma_memcpy_addr(uint64_t dst_addr, uint64_t src_addr, size_t size)
{
    uint8_t* dst_ptr = addr_to_ptr(dst_addr);
    uint8_t* src_ptr = addr_to_ptr(src_addr);
    
    if (dst_ptr && src_ptr && validate_address(src_addr, size) && validate_address(dst_addr, size)) {
        memcpy(dst_ptr, src_ptr, size);
    }
}

void dma_start_transfer(int tile_id, uint64_t src_addr, uint64_t dst_addr, size_t size)
{
    // Use the updated dma_local_transfer function
    dma_local_transfer(tile_id, src_addr, dst_addr, size);
}
