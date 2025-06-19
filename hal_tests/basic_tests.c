#include <stdio.h>
#include <string.h>
#include "basic_tests.h"
#include "hal_tests/hal_interface.h"

static void dump32(const char* tag, const uint8_t* buf){
    printf("%s 0x", tag);
    for(int i=0;i<32;i++) printf("%02X", buf[i]);
    printf(" ...\n");
}

static void banner(const char *msg)
{
    printf("################################\n");
    printf("# %s\n", msg);
    printf("################################\n");
}

int test_cpu_local_move(mesh_platform_t* p){
    const size_t bytes = 256;
    
    // Use addresses from memory map
    uint64_t src_addr = TILE0_DLM1_512_BASE;
    uint64_t dst_addr = TILE0_DLM1_512_BASE + 256;

    banner("cpu_local_move");
    
    // Setup test data using HAL memory functions (proper flow)
    g_hal.memory_fill(src_addr, 0x55, bytes);
    g_hal.memory_set(dst_addr, 0, bytes);

    // Read data using HAL for display
    uint8_t src_buffer[32], dst_buffer[32];
    g_hal.memory_read(src_addr, src_buffer, 32);
    g_hal.memory_read(dst_addr, dst_buffer, 32);
    
    dump32("[SRC-BEFORE]", src_buffer);
    dump32("[DST-BEFORE]", dst_buffer);

    // Call HAL with addresses (this is the interface under test)
    g_hal.cpu_local_move(src_addr, dst_addr, bytes);

    // Read data using HAL for verification
    g_hal.memory_read(src_addr, src_buffer, 32);
    g_hal.memory_read(dst_addr, dst_buffer, 32);
    
    dump32("[SRC-AFTER ]", src_buffer);
    dump32("[DST-AFTER ]", dst_buffer);

    // Verify using HAL memory read
    uint8_t src_verify[bytes], dst_verify[bytes];
    g_hal.memory_read(src_addr, src_verify, bytes);
    g_hal.memory_read(dst_addr, dst_verify, bytes);
    
    int ok = memcmp(src_verify, dst_verify, bytes) == 0;
    printf("[Test] CPU local move: %s\n", ok ? "PASS" : "FAIL");
    printf("\n");
    return ok;
}

int test_dma_local_transfer(mesh_platform_t* p){
    const size_t bytes = 256;
    
    // Use addresses from memory map
    uint64_t src_addr = TILE1_DLM1_512_BASE;
    uint64_t dst_addr = TILE1_DLM1_512_BASE + 256;

    banner("dma_local_transfer");
    
    // Setup test data using HAL memory functions (proper flow)
    g_hal.memory_fill(src_addr, 0xAA, bytes);
    g_hal.memory_set(dst_addr, 0, bytes);

    // Read data using HAL for display
    uint8_t src_buffer[32], dst_buffer[32];
    g_hal.memory_read(src_addr, src_buffer, 32);
    g_hal.memory_read(dst_addr, dst_buffer, 32);

    dump32("[SRC-BEFORE]  Node1.DLM1_512", src_buffer);
    dump32("[DST-BEFORE]  Node1.DLM1_512+256", dst_buffer);

    // Call HAL with addresses (proper interface)
    int result = g_hal.dma_local_transfer(1, src_addr, dst_addr, bytes);

    // Read data using HAL for verification
    g_hal.memory_read(src_addr, src_buffer, 32);
    g_hal.memory_read(dst_addr, dst_buffer, 32);

    dump32("[SRC-AFTER ]  Node1.DLM1_512", src_buffer);
    dump32("[DST-AFTER ]  Node1.DLM1_512+256", dst_buffer);

    // Verify using HAL memory read
    uint8_t src_verify[bytes], dst_verify[bytes];
    g_hal.memory_read(src_addr, src_verify, bytes);
    g_hal.memory_read(dst_addr, dst_verify, bytes);

    int ok = memcmp(src_verify, dst_verify, bytes) == 0;
    printf("[Test] DMA local transfer: %s (HAL result: %d)\n", ok ? "PASS" : "FAIL", result);
    printf("\n");  
    return ok;
}

int test_dma_remote_transfer(mesh_platform_t* p){
    const size_t bytes = 256;
    
    // Use addresses from memory map  
    uint64_t src_addr = TILE2_DLM1_512_BASE;
    uint64_t dst_addr = DMEM5_512_BASE;

    banner("dma_remote_transfer");
    
    // Setup test data using HAL memory functions (proper flow)
    g_hal.memory_fill(src_addr, 0x5A, bytes);
    g_hal.memory_set(dst_addr, 0, bytes);

    // Read data using HAL for display
    uint8_t src_buffer[32], dst_buffer[32];
    g_hal.memory_read(src_addr, src_buffer, 32);
    g_hal.memory_read(dst_addr, dst_buffer, 32);

    dump32("[SRC-BEFORE]  Node2.DLM1_512", src_buffer);
    dump32("[DST-BEFORE]  DMEM5", dst_buffer);

    // Call HAL with addresses (proper interface)
    int result = g_hal.dma_remote_transfer(src_addr, dst_addr, bytes);

    // Read data using HAL for verification
    g_hal.memory_read(src_addr, src_buffer, 32);
    g_hal.memory_read(dst_addr, dst_buffer, 32);

    dump32("[SRC-AFTER ]  Node2.DLM1_512", src_buffer);
    dump32("[DST-AFTER ]  DMEM5", dst_buffer);

    // Verify using HAL memory read
    uint8_t src_verify[bytes], dst_verify[bytes];
    g_hal.memory_read(src_addr, src_verify, bytes);
    g_hal.memory_read(dst_addr, dst_verify, bytes);

    int ok = memcmp(src_verify, dst_verify, bytes) == 0;
    printf("[Test] DMA remote transfer: %s (HAL result: %d)\n", ok ? "PASS" : "FAIL", result);
    printf("\n");
    return ok;
}
