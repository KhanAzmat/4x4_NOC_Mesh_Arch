#ifndef TILE_DMA_H
#define TILE_DMA_H
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t id;
} dma_engine_t;

// Legacy pointer-based DMA function
void dma_memcpy(void* dst, const void* src, size_t size);

// New address-based DMA functions
void dma_memcpy_addr(uint64_t dst_addr, uint64_t src_addr, size_t size);
void dma_start_transfer(int tile_id, uint64_t src_addr, uint64_t dst_addr, size_t size);

int dma_local_transfer(int tile_id, uint64_t src_addr, uint64_t dst_addr, size_t size);

#endif
