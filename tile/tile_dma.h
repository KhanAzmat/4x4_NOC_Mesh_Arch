#ifndef TILE_DMA_H
#define TILE_DMA_H
#include <stddef.h>
#include <stdint.h>
#include "hal/dma512/hal_dmac512.h"

typedef struct {
    uint32_t id;
} dma_engine_t;

// DMAC512 integration functions
int dma_tile_init(int tile_id);
int dma_tile_get_handle(int tile_id, DMAC512_HandleTypeDef** handle);

// Updated DMA functions using DMAC512
int dma_local_transfer(int tile_id, uint64_t src_addr, uint64_t dst_addr, size_t size);

// Legacy functions for backward compatibility
void dma_memcpy(void* dst, const void* src, size_t size);
void dma_memcpy_addr(uint64_t dst_addr, uint64_t src_addr, size_t size);
void dma_start_transfer(int tile_id, uint64_t src_addr, uint64_t dst_addr, size_t size);

#endif
