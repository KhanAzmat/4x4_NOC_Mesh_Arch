#include <stdio.h>
#include <stdlib.h>
#include "hal_tests/hal_interface.h"
#include "platform_init/address_manager.h"
#include "c0_master/c0_controller.h"
#include <string.h>
#include "tile_dma.h"
#include "mesh_noc/mesh_router.h"
#include "mesh_noc/noc_packet.h"
#include "dmem/dmem_controller.h"
#include "hal_dmac512.h"  // Phase 3: DMAC512 HAL integration
#include "generated/mem_map.h"  // For NUM_TILES constant
#include <pthread.h>
#include <unistd.h>
#include "dmac512_hardware_monitor.h"

// Remove the HAL bypass - let HAL/Driver execute normally

static mesh_platform_t* g_platform = NULL;

// Thread safety for HAL interface
static pthread_mutex_t hal_mutex = PTHREAD_MUTEX_INITIALIZER;

// HAL Function Call Tracking
void hal_function_entry(const char* hal_func, const char* caller_test) {
    printf("[HAL-ENTRY] %s called by test '%s'\n", hal_func, caller_test);
    fflush(stdout);
}

void hal_function_exit(const char* hal_func, int result) {
    printf("[HAL-EXIT] %s completed with result: %d\n", hal_func, result);
    fflush(stdout);
}

void hal_set_platform(mesh_platform_t* p) { g_platform = p; }

// Helper function to monitor DMA registers after HAL operations
static void monitor_dma_after_hal(DMAC512_HandleTypeDef* dmac_handle) {
    if (!dmac_handle || !dmac_handle->Instance) {
        return;
    }
    
    extern mesh_platform_t* global_platform_ptr;
    if (!global_platform_ptr) {
        return;
    }
    
    // Find which tile this DMAC handle belongs to
    int tile_id = platform_get_tile_id_from_dmac_regs(dmac_handle->Instance);
    if (tile_id >= 0) {
        // Check if HAL just enabled DMA and execute transfer immediately
        uint32_t xfer_cnt_reg = dmac_handle->Instance->DMAC_TOTAL_XFER_CNT;
        bool dma_enabled = (xfer_cnt_reg & DMAC512_TOTAL_XFER_CNT_DMAC_EN_MASK) != 0;
        
        if (dma_enabled) {
            printf("[DMAC512-POST-HAL] Tile %d: HAL enabled DMA, executing transfer immediately\n", tile_id);
            dmac512_execute_on_enable_write(tile_id, global_platform_ptr, dmac_handle->Instance);
        }
    }
}

static int ref_cpu_local_move(uint64_t src_addr, uint64_t dst_addr, size_t size)
{
    hal_function_entry("hal_cpu_local_move", "CPU Local Move Test");
    
    pthread_mutex_lock(&hal_mutex);
    
    // HAL validates addresses and translates to memory access
    uint8_t* src_ptr = addr_to_ptr(src_addr);
    uint8_t* dst_ptr = addr_to_ptr(dst_addr);
    
    if (!src_ptr || !dst_ptr) {
        pthread_mutex_unlock(&hal_mutex);
        hal_function_exit("hal_cpu_local_move", -1);
        return -1;
    }
    if (!validate_address(src_addr, size) || !validate_address(dst_addr, size)) {
        pthread_mutex_unlock(&hal_mutex);
        hal_function_exit("hal_cpu_local_move", -1);
        return -1;
    }
    
    printf("[DRIVER-CALL] CPU Local Move → memory driver (memmove)\n");
    fflush(stdout);
    
    // HAL could call driver here, or do direct memory access
    memmove(dst_ptr, src_ptr, size);
    
    pthread_mutex_unlock(&hal_mutex);
    hal_function_exit("hal_cpu_local_move", 0);
    return 0;
}

static int ref_dma_local_transfer(int tile_id, uint64_t src_addr, uint64_t dst_addr, size_t size)
{
    hal_function_entry("hal_dma_local_transfer", "DMA Local Transfer Test");
    
    pthread_mutex_lock(&hal_mutex);
    
    if (!g_platform || tile_id < 0 || tile_id >= NUM_TILES) {
        pthread_mutex_unlock(&hal_mutex);
        hal_function_exit("hal_dma_local_transfer", -1);
        return -1;
    }
    
    printf("[DRIVER-CALL] DMA Local Transfer → DMAC512 HAL driver\n");
    fflush(stdout);
    
    // Phase 3: Use DMAC512 HAL instead of basic DMA
    DMAC512_HandleTypeDef* dmac_handle = &g_platform->nodes[tile_id].dmac512_handle;
    
    // Configure DMAC512 for the transfer
    dmac_handle->Init.SrcAddr = src_addr;
    dmac_handle->Init.DstAddr = dst_addr;
    dmac_handle->Init.XferCount = (uint32_t)size;
    dmac_handle->Init.dob_beat = DMAC512_AXI_TRANS_4;  // Default 4-beat transfers
    dmac_handle->Init.dfb_beat = DMAC512_AXI_TRANS_4;
    dmac_handle->Init.DmacMode = DMAC512_NORMAL_MODE;
    
    printf("[DMAC512-HAL] Tile %d: Configuring transfer (src=0x%lX, dst=0x%lX, size=%zu)\n",
           tile_id, src_addr, dst_addr, size);
    fflush(stdout);
    
    // Configure the channel (writes to registers)
    int32_t config_result = HAL_DMAC512ConfigureChannel(dmac_handle);
    if (config_result != 0) {
        printf("[DMAC512-HAL] Tile %d: Configuration failed\n", tile_id);
        pthread_mutex_unlock(&hal_mutex);
        hal_function_exit("hal_dma_local_transfer", -1);
        return -1;
    }
    
    printf("[DMAC512-HAL] Tile %d: Starting transfer...\n", tile_id);
    fflush(stdout);
    
    // Start the transfer (writes enable bit - should trigger Phase 2 detection)
    HAL_DMAC512StartTransfers(dmac_handle);
    
    // Monitor DMA registers after HAL operation and execute transfer immediately if enabled
    monitor_dma_after_hal(dmac_handle);
    
    // Wait for transfer completion (poll busy status)
    int timeout = 1000; // 1000 iterations max
    while (HAL_DMAC512IsBusy(dmac_handle) && timeout > 0) {
        usleep(100); // 100 microseconds
        timeout--;
    }
    
    if (timeout == 0) {
        printf("[DMAC512-HAL] Tile %d: Transfer timeout\n", tile_id);
        pthread_mutex_unlock(&hal_mutex);
        hal_function_exit("hal_dma_local_transfer", -1);
        return -1;
    }
    
    printf("[DMAC512-HAL] Tile %d: Transfer completed successfully\n", tile_id);
    
    pthread_mutex_unlock(&hal_mutex);
    hal_function_exit("hal_dma_local_transfer", (int)size);
    return (int)size;
}

static int ref_dma_remote_transfer(uint64_t src_addr, uint64_t dst_addr, size_t size)
{
    hal_function_entry("hal_dma_remote_transfer", "DMA Remote Transfer Test");
    
    pthread_mutex_lock(&hal_mutex);
    
    if (!g_platform) {
        pthread_mutex_unlock(&hal_mutex);
        hal_function_exit("hal_dma_remote_transfer", -1);
        return -1;
    }
    if (!validate_address(src_addr, size) || !validate_address(dst_addr, size)) {
        pthread_mutex_unlock(&hal_mutex);
        hal_function_exit("hal_dma_remote_transfer", -1);
        return -1;
    }
    
    // Validate this is a valid remote transfer (tile<->dmem)
    addr_region_t src_region = get_address_region(src_addr);
    addr_region_t dst_region = get_address_region(dst_addr);
    
    // Allow tile DLM1_512 <-> DMEM transfers
    if (!((src_region == ADDR_TILE_DLM1_512 && dst_region == ADDR_DMEM_512) ||
          (src_region == ADDR_DMEM_512 && dst_region == ADDR_TILE_DLM1_512))) {
        pthread_mutex_unlock(&hal_mutex);
        hal_function_exit("hal_dma_remote_transfer", -1);
        return -1;
    }
    
    printf("[DRIVER-CALL] DMA Remote Transfer → DMAC512 HAL driver\n");
    fflush(stdout);
    
    // Phase 3: Determine which tile's DMAC512 to use for remote transfer
    int tile_id = -1;
    if (src_region == ADDR_TILE_DLM1_512) {
        tile_id = get_tile_id_from_address(src_addr);
    } else if (dst_region == ADDR_TILE_DLM1_512) {
        tile_id = get_tile_id_from_address(dst_addr);
    }
    
    if (tile_id < 0 || tile_id >= NUM_TILES) {
        // Fallback to tile 0 for DMEM-to-DMEM or unknown cases
        tile_id = 0;
    }
    
    printf("[DMAC512-HAL] Using Tile %d DMAC512 for remote transfer\n", tile_id);
    fflush(stdout);
    
    // Phase 3: Use DMAC512 HAL instead of direct NoC packets
    DMAC512_HandleTypeDef* dmac_handle = &g_platform->nodes[tile_id].dmac512_handle;
    
    // Configure DMAC512 for the remote transfer
    dmac_handle->Init.SrcAddr = src_addr;
    dmac_handle->Init.DstAddr = dst_addr;
    dmac_handle->Init.XferCount = (uint32_t)size;
    dmac_handle->Init.dob_beat = DMAC512_AXI_TRANS_4;  // Default 4-beat transfers
    dmac_handle->Init.dfb_beat = DMAC512_AXI_TRANS_4;
    dmac_handle->Init.DmacMode = DMAC512_NORMAL_MODE;
    
    printf("[DMAC512-HAL] Tile %d: Configuring remote transfer (src=0x%lX, dst=0x%lX, size=%zu)\n",
           tile_id, src_addr, dst_addr, size);
    fflush(stdout);
    
    // Configure the channel (writes to registers)
    int32_t config_result = HAL_DMAC512ConfigureChannel(dmac_handle);
    if (config_result != 0) {
        printf("[DMAC512-HAL] Tile %d: Configuration failed\n", tile_id);
        pthread_mutex_unlock(&hal_mutex);
        hal_function_exit("hal_dma_remote_transfer", -1);
        return -1;
    }
    
    printf("[DMAC512-HAL] Tile %d: Starting remote transfer...\n", tile_id);
    fflush(stdout);
    
    // Start the transfer (writes enable bit - should trigger Phase 2 detection)
    HAL_DMAC512StartTransfers(dmac_handle);
    
    // Monitor DMA registers after HAL operation and execute transfer immediately if enabled
    monitor_dma_after_hal(dmac_handle);
    
    // Wait for transfer completion (poll busy status)
    int timeout = 1000; // 1000 iterations max
    while (HAL_DMAC512IsBusy(dmac_handle) && timeout > 0) {
        usleep(100); // 100 microseconds
        timeout--;
    }
    
    if (timeout == 0) {
        printf("[DMAC512-HAL] Tile %d: Remote transfer timeout\n", tile_id);
        pthread_mutex_unlock(&hal_mutex);
        hal_function_exit("hal_dma_remote_transfer", -1);
        return -1;
    }
    
    printf("[DMAC512-HAL] Tile %d: Remote transfer completed successfully\n", tile_id);
    
    pthread_mutex_unlock(&hal_mutex);
    hal_function_exit("hal_dma_remote_transfer", (int)size);
    return (int)size;
}

static int ref_dmem_to_dmem_transfer(uint64_t src_addr, uint64_t dst_addr, size_t size)
{
    pthread_mutex_lock(&hal_mutex);
    
    if (!g_platform) {
        pthread_mutex_unlock(&hal_mutex);
        return -1;
    }
    if (!validate_address(src_addr, size) || !validate_address(dst_addr, size)) {
        pthread_mutex_unlock(&hal_mutex);
        return -1;
    }
        
    // Validate both addresses are DMEM regions
    addr_region_t src_region = get_address_region(src_addr);
    addr_region_t dst_region = get_address_region(dst_addr);
    
    if (src_region != ADDR_DMEM_512 || dst_region != ADDR_DMEM_512) {
        pthread_mutex_unlock(&hal_mutex);
        return -1;
    }
    
    // HAL calls DMEM driver instead of NoC for DMEM-to-DMEM transfers
    // This follows the proper Tests → HAL → Driver flow
    int result = dmem_copy(src_addr, dst_addr, size);
    
    pthread_mutex_unlock(&hal_mutex);
    return result;
}

static int ref_node_sync(int mask) { 
    pthread_mutex_lock(&hal_mutex);
    (void)mask; 
    pthread_mutex_unlock(&hal_mutex);
    return 0; 
}

static int ref_get_dmem_status(uint64_t dmem_base_addr) { 
    pthread_mutex_lock(&hal_mutex);
    
    // Validate it's a DMEM address
    if (get_address_region(dmem_base_addr) != ADDR_DMEM_512) {
        pthread_mutex_unlock(&hal_mutex);
        return -1;
    }
    
    // HAL calls DMEM driver for status
    int dmem_id = get_dmem_id_from_address(dmem_base_addr);
    if (dmem_id < 0) {
        pthread_mutex_unlock(&hal_mutex);
        return -1;
    }
    
    int result = dmem_get_status(dmem_id);
    pthread_mutex_unlock(&hal_mutex);
    return result;
}

static int ref_mesh_route_optimal(uint64_t src_addr, uint64_t dst_addr) { 
    pthread_mutex_lock(&hal_mutex);
    
    // Calculate mesh coordinates from addresses
    int src_tile = get_tile_id_from_address(src_addr);
    int dst_tile = get_tile_id_from_address(dst_addr);
    
    if (src_tile < 0 || dst_tile < 0) {
        pthread_mutex_unlock(&hal_mutex);
        return -1;
    }
    
    // Simple Manhattan distance calculation
    int src_x = src_tile % 4, src_y = src_tile / 4;
    int dst_x = dst_tile % 4, dst_y = dst_tile / 4;
    
    int result = abs(dst_x - src_x) + abs(dst_y - src_y);
    pthread_mutex_unlock(&hal_mutex);
    return result;
}

// Memory access functions for test setup/verification - use proper drivers
static int ref_memory_read(uint64_t addr, uint8_t* buffer, size_t size) {
    pthread_mutex_lock(&hal_mutex);
    
    if (!buffer || size == 0) {
        pthread_mutex_unlock(&hal_mutex);
        return -1;
    }
    if (!validate_address(addr, size)) {
        pthread_mutex_unlock(&hal_mutex);
        return -1;
    }
    
    // Use address manager (base symbol layer)
    uint8_t* src_ptr = addr_to_ptr(addr);
    if (!src_ptr) {
        pthread_mutex_unlock(&hal_mutex);
        return -1;
    }
    
    memcpy(buffer, src_ptr, size);
    pthread_mutex_unlock(&hal_mutex);
    return (int)size;
}

static int ref_memory_write(uint64_t addr, const uint8_t* buffer, size_t size) {
    pthread_mutex_lock(&hal_mutex);
    
    if (!buffer || size == 0) {
        pthread_mutex_unlock(&hal_mutex);
        return -1;
    }
    if (!validate_address(addr, size)) {
        pthread_mutex_unlock(&hal_mutex);
        return -1;
    }
    
    // Use address manager (base symbol layer)
    uint8_t* dst_ptr = addr_to_ptr(addr);
    if (!dst_ptr) {
        pthread_mutex_unlock(&hal_mutex);
        return -1;
    }
    
    memcpy(dst_ptr, buffer, size);
    pthread_mutex_unlock(&hal_mutex);
    return (int)size;
}

static int ref_memory_fill(uint64_t addr, uint8_t value, size_t size) {
    pthread_mutex_lock(&hal_mutex);
    
    if (size == 0) {
        pthread_mutex_unlock(&hal_mutex);
        return -1;
    }
    if (!validate_address(addr, size)) {
        pthread_mutex_unlock(&hal_mutex);
        return -1;
    }
    
    // Use address manager (base symbol layer)
    uint8_t* dst_ptr = addr_to_ptr(addr);
    if (!dst_ptr) {
        pthread_mutex_unlock(&hal_mutex);
        return -1;
    }
    
    // Create pattern based on value
    for (size_t i = 0; i < size; i++) {
        dst_ptr[i] = (uint8_t)(value + i);
    }
    pthread_mutex_unlock(&hal_mutex);
    return (int)size;
}

static int ref_memory_set(uint64_t addr, uint8_t value, size_t size) {
    pthread_mutex_lock(&hal_mutex);
    
    if (size == 0) {
        pthread_mutex_unlock(&hal_mutex);
        return -1;
    }
    if (!validate_address(addr, size)) {
        pthread_mutex_unlock(&hal_mutex);
        return -1;
    }
    
    // Use address manager (base symbol layer)
    uint8_t* dst_ptr = addr_to_ptr(addr);
    if (!dst_ptr) {
        pthread_mutex_unlock(&hal_mutex);
        return -1;
    }
    
    memset(dst_ptr, value, size);
    pthread_mutex_unlock(&hal_mutex);
    return (int)size;
}

hal_interface_t g_hal;

void hal_use_reference_impl(void)
{
    g_hal.cpu_local_move       = ref_cpu_local_move;
    g_hal.dma_local_transfer   = ref_dma_local_transfer;
    g_hal.dma_remote_transfer  = ref_dma_remote_transfer;
    g_hal.dmem_to_dmem_transfer= ref_dmem_to_dmem_transfer;
    g_hal.node_sync            = ref_node_sync;
    g_hal.get_dmem_status      = ref_get_dmem_status;
    g_hal.mesh_route_optimal   = ref_mesh_route_optimal;
    g_hal.memory_read          = ref_memory_read;
    g_hal.memory_write         = ref_memory_write;
    g_hal.memory_fill          = ref_memory_fill;
    g_hal.memory_set           = ref_memory_set;
}
