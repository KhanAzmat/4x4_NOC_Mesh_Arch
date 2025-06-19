#include <string.h>
#include "tile_dma.h"
#include "platform_init/address_manager.h"

void dma_memcpy(void* dst, const void* src, size_t size)
{
    /* blocking copy for simulation */
    memcpy(dst, src, size);
    // return (int)size;
}

int dma_local_transfer(int tile_id, uint64_t src_addr, uint64_t dst_addr, size_t size){
    // Driver validates addresses are within the same tile
    if(get_tile_id_from_address(src_addr) != tile_id || 
    get_tile_id_from_address(dst_addr) != tile_id){
        return -1;
    }

    // Driver translates addresses to pointers
    uint8_t* src_ptr = addr_to_ptr(src_addr);
    uint8_t* dst_ptr = addr_to_ptr(dst_addr);

    if(!src_ptr || !dst_ptr) return -1;

    if(!validate_address(src_addr, size) || !validate_address(dst_addr, size)) return -1;

    // dma_memcpy(dst_ptr, src_ptr, size);
    memcpy(dst_ptr, src_ptr, size);
    return (int)size;
}

void dma_memcpy_addr(uint64_t dst_addr, uint64_t src_addr, size_t size)
{
    uint8_t* dst_ptr = addr_to_ptr(dst_addr);
    uint8_t* src_ptr = addr_to_ptr(src_addr);
    
    if (dst_ptr && src_ptr && validate_address(src_addr, size) && validate_address(dst_addr, size)) {
        memcpy(dst_ptr, src_ptr, size);
    }
}

int dma_local_copy(int tile_id, uint64_t src_addr, uint64_t dst_addr, size_t size)
{
    // Driver translates addresses to hardware access
    uint8_t* src_ptr = addr_to_ptr(src_addr);
    uint8_t* dst_ptr = addr_to_ptr(dst_addr);
    
    if (!src_ptr || !dst_ptr) return -1;
    
    // Simulate DMA operation
    memcpy(dst_ptr, src_ptr, size);
    return size;
}

// For more complex operations, simulate DMA registers
// int dma_start_transfer(int tile_id, uint64_t src, uint64_t dst, size_t size)
// {
//     // Get DMA register base for this tile
//     uint64_t dma_base = get_tile_dma_base(tile_id);
    
//     // Write to simulated DMA control registers
//     dma_write_reg(dma_base + DMA_SRC_ADDR_REG, src);
//     dma_write_reg(dma_base + DMA_DST_ADDR_REG, dst);
//     dma_write_reg(dma_base + DMA_SIZE_REG, size);
//     dma_write_reg(dma_base + DMA_CTRL_REG, DMA_START_BIT);
    
//     // Simulate transfer completion
//     return dma_local_copy(tile_id, src, dst, size);
// }
