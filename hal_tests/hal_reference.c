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
#include <pthread.h>

static mesh_platform_t* g_platform = NULL;

// Thread safety for HAL interface
static pthread_mutex_t hal_mutex = PTHREAD_MUTEX_INITIALIZER;

void hal_set_platform(mesh_platform_t* p) { g_platform = p; }

static int ref_cpu_local_move(uint64_t src_addr, uint64_t dst_addr, size_t size)
{
    pthread_mutex_lock(&hal_mutex);
    
    // HAL validates addresses and translates to memory access
    uint8_t* src_ptr = addr_to_ptr(src_addr);
    uint8_t* dst_ptr = addr_to_ptr(dst_addr);
    
    if (!src_ptr || !dst_ptr) {
        pthread_mutex_unlock(&hal_mutex);
        return -1;
    }
    if (!validate_address(src_addr, size) || !validate_address(dst_addr, size)) {
        pthread_mutex_unlock(&hal_mutex);
        return -1;
    }
    
    // HAL could call driver here, or do direct memory access
    memmove(dst_ptr, src_ptr, size);
    
    pthread_mutex_unlock(&hal_mutex);
    return 0;
}

static int ref_dma_local_transfer(int tile_id, uint64_t src_addr, uint64_t dst_addr, size_t size)
{
    pthread_mutex_lock(&hal_mutex);
    
    // Call tile DMA driver
    int result = dma_local_transfer(tile_id, src_addr, dst_addr, size);
    
    pthread_mutex_unlock(&hal_mutex);
    return result;
}

static int ref_dma_remote_transfer(uint64_t src_addr, uint64_t dst_addr, size_t size)
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

    
    
    // Validate this is a valid remote transfer (tile<->dmem)
    addr_region_t src_region = get_address_region(src_addr);
    addr_region_t dst_region = get_address_region(dst_addr);
    
    // Allow tile DLM1_512 <-> DMEM transfers
    if (!((src_region == ADDR_TILE_DLM1_512 && dst_region == ADDR_DMEM_512) ||
          (src_region == ADDR_DMEM_512 && dst_region == ADDR_TILE_DLM1_512))) {
        pthread_mutex_unlock(&hal_mutex);
        return -1;
    }
    
    // Create NoC packet with address information
    noc_packet_t pkt = {0};
    int src_tile = get_tile_id_from_address(src_addr);
    int dst_tile = get_tile_id_from_address(dst_addr);
    
    if (src_tile >= 0) {
        pkt.hdr.src_x = src_tile % 4;
        pkt.hdr.src_y = src_tile / 4;
    }
    if (dst_tile >= 0) {
        pkt.hdr.dest_x = dst_tile % 4;
        pkt.hdr.dest_y = dst_tile / 4;
    }
    
    pkt.hdr.type = PKT_DMA_TRANSFER;
    pkt.hdr.length = (uint16_t)size;
    pkt.hdr.src_addr = src_addr;  // Add source address
    pkt.hdr.dst_addr = dst_addr;  // Add destination address
    
    // NoC driver now does both routing AND data transfer
    noc_send_packet(&pkt);
    
    pthread_mutex_unlock(&hal_mutex);
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
