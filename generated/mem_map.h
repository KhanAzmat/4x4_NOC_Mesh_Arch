// #ifndef MEM_MAP_H
// #define MEM_MAP_H

// /* Auto‑generated flat address map for nodes + DMEMs                */
// /* NOTE:  This simulation uses indexes rather than true addresses. */

// enum {
//     NODE0 = 0, NODE1, NODE2, NODE3,
//     NODE4, NODE5, NODE6, NODE7
// };

// enum {
//     DMEM0 = 0, DMEM1, DMEM2, DMEM3,
//     DMEM4, DMEM5, DMEM6, DMEM7
// };

// #endif /* MEM_MAP_H */



// generated/mem_map.h
// Auto-generated memory map for 4x4 Mesh NoC Test Platform
// Used by HAL, driver stack, DMA engine, and simulation platform

#ifndef MEM_MAP_H
#define MEM_MAP_H

#include <stdint.h>

// ------------------------------
// Mesh and System Configuration
// ------------------------------
#define MESH_SIZE_X        4
#define MESH_SIZE_Y        4
#define NUM_TILES          8
#define NUM_DMEMS          8

// ------------------------------
// Address Region Strides
// ------------------------------
#define TILE_STRIDE        0x00100000UL  // 1MB per tile
#define DMEM_STRIDE        0x00400000UL  // 4MB per DMEM

// ------------------------------
// Memory Sizes
// ------------------------------
#define DLM_64_SIZE        0x00008000UL  // 32 KiB
#define DLM1_512_SIZE      0x00020000UL  // 128 KiB
#define DMEM_512_SIZE      0x00040000UL  // 256 KiB

// ------------------------------
// Offsets within Tile
// ------------------------------
#define DLM_64_OFFSET      0x0000UL      // 32 KiB scratchpad
#define DLM1_512_OFFSET    0x8000UL      // 128 KiB buffer
#define DMA_REG_OFFSET     0xF000UL      // DMA control block

// ------------------------------
// Per-Tile Address Definitions
// ------------------------------
#define TILE0_BASE         0x00000000UL
#define TILE1_BASE         0x00100000UL
#define TILE2_BASE         0x00200000UL
#define TILE3_BASE         0x00300000UL
#define TILE4_BASE         0x00400000UL
#define TILE5_BASE         0x00500000UL
#define TILE6_BASE         0x00600000UL
#define TILE7_BASE         0x00700000UL

// DLM_64 Base Addresses
#define TILE0_DLM_64_BASE     (TILE0_BASE + DLM_64_OFFSET)
#define TILE1_DLM_64_BASE     (TILE1_BASE + DLM_64_OFFSET)
#define TILE2_DLM_64_BASE     (TILE2_BASE + DLM_64_OFFSET)
#define TILE3_DLM_64_BASE     (TILE3_BASE + DLM_64_OFFSET)
#define TILE4_DLM_64_BASE     (TILE4_BASE + DLM_64_OFFSET)
#define TILE5_DLM_64_BASE     (TILE5_BASE + DLM_64_OFFSET)
#define TILE6_DLM_64_BASE     (TILE6_BASE + DLM_64_OFFSET)
#define TILE7_DLM_64_BASE     (TILE7_BASE + DLM_64_OFFSET)

// DLM1_512 Base Addresses
#define TILE0_DLM1_512_BASE   (TILE0_BASE + DLM1_512_OFFSET)
#define TILE1_DLM1_512_BASE   (TILE1_BASE + DLM1_512_OFFSET)
#define TILE2_DLM1_512_BASE   (TILE2_BASE + DLM1_512_OFFSET)
#define TILE3_DLM1_512_BASE   (TILE3_BASE + DLM1_512_OFFSET)
#define TILE4_DLM1_512_BASE   (TILE4_BASE + DLM1_512_OFFSET)
#define TILE5_DLM1_512_BASE   (TILE5_BASE + DLM1_512_OFFSET)
#define TILE6_DLM1_512_BASE   (TILE6_BASE + DLM1_512_OFFSET)
#define TILE7_DLM1_512_BASE   (TILE7_BASE + DLM1_512_OFFSET)

// DMA Register Block Base Addresses
#define TILE0_DMA_REG_BASE    (TILE0_BASE + DMA_REG_OFFSET)
#define TILE1_DMA_REG_BASE    (TILE1_BASE + DMA_REG_OFFSET)
#define TILE2_DMA_REG_BASE    (TILE2_BASE + DMA_REG_OFFSET)
#define TILE3_DMA_REG_BASE    (TILE3_BASE + DMA_REG_OFFSET)
#define TILE4_DMA_REG_BASE    (TILE4_BASE + DMA_REG_OFFSET)
#define TILE5_DMA_REG_BASE    (TILE5_BASE + DMA_REG_OFFSET)
#define TILE6_DMA_REG_BASE    (TILE6_BASE + DMA_REG_OFFSET)
#define TILE7_DMA_REG_BASE    (TILE7_BASE + DMA_REG_OFFSET)

// ------------------------------
// DMEM Base Addresses
// ------------------------------
#define DMEM0_512_BASE        0x10000000UL
#define DMEM1_512_BASE        0x10400000UL
#define DMEM2_512_BASE        0x10800000UL
#define DMEM3_512_BASE        0x10C00000UL
#define DMEM4_512_BASE        0x20000000UL
#define DMEM5_512_BASE        0x20400000UL
#define DMEM6_512_BASE        0x20800000UL
#define DMEM7_512_BASE        0x20C00000UL

// ------------------------------
// C0 Master Control Region
// ------------------------------
#define C0_MASTER_BASE        0x80000000UL
#define C0_MASTER_SIZE        0x00010000UL  // 64 KiB

// ------------------------------
// NoC Interface Parameters
// ------------------------------
#define NOC_LINK_WIDTH_BITS   512
#define NOC_PACKET_HEADER_SIZE 32   // bytes
#define NOC_MAX_PAYLOAD_BYTES  64   // bytes (512 bits)

#endif // MEM_MAP_H
