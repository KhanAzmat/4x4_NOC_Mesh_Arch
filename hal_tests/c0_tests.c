


// c0_tests.c – single‑test gather and distribute for C0 master
// This file replaces the previous version to meet the exact spec:
//  * C0‑Gather   : read eight 256‑byte blocks from DMEM0‑7 -> node_0.dlm1
//  * C0‑Distribute: write the same eight blocks from node_0.dlm1 -> DMEM0‑7
//  * Each is ONE logical test; banners added for clarity

#include <stdio.h>
#include <string.h>

#include "c0_tests.h"
#include "hal_tests/hal_interface.h"

/* ───────────────── helpers ───────────────── */
#define CHUNK 256
static void banner(const char *msg)
{
    printf("################################\n");
    printf("# %s\n", msg);
    printf("################################\n");
}
static void dump32(const char *tag, const uint8_t *buf)
{
    printf("%s 0x", tag);
    for (int i = 0; i < 32; ++i) printf("%02X", buf[i]);
    printf(" ...\n");
}
static void fill(uint8_t *buf, uint8_t base)
{
    for (size_t i = 0; i < CHUNK; ++i) buf[i] = (uint8_t)(base + i);
}

/* ─────────────────────────────────────────────
 *  C0‑Gather  (single logical test)
 * ───────────────────────────────────────────*/
int test_c0_gather(mesh_platform_t *p)
{
    banner("C0-Gather(collect 8 DMEM to a continue DLM1)");

    /* 1. Seed DMEMs with deterministic data using HAL functions */
    for (int d = 0; d < 8; ++d) {
        uint64_t dmem_addr = p->dmems[d].dmem_base_addr;
        g_hal.memory_fill(dmem_addr, (uint8_t)(0x10 | d), CHUNK);
    }
    int pass = 0;

    /* 2. C0 sequentially reads each DMEM into successive slices using HAL */
    for (int d = 0; d < 8; ++d) {
        uint64_t src_addr = p->dmems[d].dmem_base_addr;
        uint64_t dst_addr = p->nodes[0].dlm1_512_base_addr + d * CHUNK;

        printf("%d. HAL transfer: dmem_%d(0x%lx) -> node_0.dlm1+%d(0x%lx)\n", 
               d + 1, d, src_addr, d * CHUNK, dst_addr);
        
        // Read data using HAL for display
        uint8_t src_buffer[32], dst_buffer[32];
        g_hal.memory_read(src_addr, src_buffer, 32);
        g_hal.memory_read(dst_addr, dst_buffer, 32);
        
        dump32("[SRC-BEFORE]", src_buffer);
        dump32("[DST-BEFORE]", dst_buffer);

        /* Use HAL for remote transfer (DMEM -> Tile) */
        int result = g_hal.dma_remote_transfer(src_addr, dst_addr, CHUNK);

        // Read data using HAL for verification
        g_hal.memory_read(dst_addr, dst_buffer, 32);
        
        dump32("[DST-AFTER ]", dst_buffer);
        printf("HAL result: %d\n\n", result);

        /* Verify transfer using HAL memory read */
        uint8_t src_verify[CHUNK], dst_verify[CHUNK];
        g_hal.memory_read(src_addr, src_verify, CHUNK);
        g_hal.memory_read(dst_addr, dst_verify, CHUNK);
        
        pass += (memcmp(src_verify, dst_verify, CHUNK) == 0);
    }

    printf("[C0-Gather] Summary: %d/8 passed\n\n", pass);
    return pass == 8;
}

/* ─────────────────────────────────────────────
 *  C0‑Distribute (single logical test, broadcast to 8 DMEMs)
 * ───────────────────────────────────────────*/
int test_c0_distribute(mesh_platform_t *p)
{
    banner("C0-Distribute(same SRC --> diff. Dist)");

    /* 1. Put unique patterns into eight slices of node_0.dlm1 using HAL functions */
    for (int d = 0; d < 8; ++d) {
        uint64_t node_addr = p->nodes[0].dlm1_512_base_addr + d * CHUNK;
        g_hal.memory_fill(node_addr, (uint8_t)(0xE0 | d), CHUNK);
    }

    int pass = 0;

    /* 2. Broadcast each slice to its corresponding DMEM using HAL */
    for (int d = 0; d < 8; ++d) {
        uint64_t src_addr = p->nodes[0].dlm1_512_base_addr;
        uint64_t dst_addr = p->dmems[d].dmem_base_addr;

        printf("%d. HAL transfer: node_0.dlm1(0x%lx) -> dmem_%d(0x%lx), size: %d\n", 
               d + 1, src_addr, d, dst_addr, CHUNK);
        
        // Read data using HAL for display
        uint8_t src_buffer[32], dst_buffer[32];
        g_hal.memory_read(src_addr, src_buffer, 32);
        
        dump32("[SRC-BEFORE]", src_buffer);

        /* Use HAL for remote transfer (Tile -> DMEM) */
        int result = g_hal.dma_remote_transfer(src_addr, dst_addr, CHUNK);

        // Read data using HAL for verification
        g_hal.memory_read(dst_addr, dst_buffer, 32);
        
        dump32("[DST-AFTER ]", dst_buffer);
        printf("HAL result: %d\n\n", result);

        /* Verify transfer using HAL memory read */
        uint8_t src_verify[CHUNK], dst_verify[CHUNK];
        g_hal.memory_read(src_addr, src_verify, CHUNK);
        g_hal.memory_read(dst_addr, dst_verify, CHUNK);
        
        pass += (memcmp(src_verify, dst_verify, CHUNK) == 0);
    }

    printf("[C0-Distribute] Summary: %d/8 passed\n\n", pass);
    return pass == 8;
}
