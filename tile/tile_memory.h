#ifndef TILE_MEMORY_H
#define TILE_MEMORY_H
#include <stdint.h>
#include "generated/mem_map.h"

typedef struct {
    uint8_t  dlm64[DLM_64_SIZE];
    uint8_t  dlm1_512[DLM1_512_SIZE];
} tile_local_mem_t;

// typedef struct {
//     uint8_t dmem[DMEM_512_SIZE];
// } dmem_module_t;

#endif
