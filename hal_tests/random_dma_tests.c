// random_dma_tests.c – two deterministic remote-DMA cases with full dumps

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdarg.h>
#include "random_dma_tests.h"
#include "hal_tests/hal_interface.h"

// Thread-safe printing for parallel test execution
static pthread_mutex_t print_mutex = PTHREAD_MUTEX_INITIALIZER;

static void thread_safe_banner(const char *msg)
{
    pthread_mutex_lock(&print_mutex);
    printf("################################\n");
    printf("# %s\n", msg);
    printf("################################\n");
    fflush(stdout);
    pthread_mutex_unlock(&print_mutex);
}

static void thread_safe_dump32(const char *tag, const uint8_t *buf)
{
    pthread_mutex_lock(&print_mutex);
    printf("%s 0x", tag);
    for (int i = 0; i < 32; ++i) printf("%02X", buf[i]);
    printf(" ...\n");
    fflush(stdout);
    pthread_mutex_unlock(&print_mutex);
}

static void thread_safe_printf(const char* format, ...)
{
    pthread_mutex_lock(&print_mutex);
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    fflush(stdout);
    pthread_mutex_unlock(&print_mutex);
}

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

static void fill(uint8_t *buf, size_t len, uint8_t seed)
{
    for (size_t i = 0; i < len; ++i) buf[i] = (uint8_t)(seed ^ i);
}

/* exactly TWO test cases → prints full before/after for EACH */
int test_random_dma_remote(mesh_platform_t *p)
{
    const size_t BYTES = 256;
    int pass_cnt = 0;

    thread_safe_banner("random_dma_remote");
	
	
    struct {
        int src_node;
        int dst_dmem;
        uint8_t seed;
    } cases[2] = {
        {0, 5, 0xA5},   // node_0 → dmem_5
        {4, 7, 0x5A}    // node_4 → dmem_7
    };

    for (int i = 0; i < 2; ++i) {
        int src = cases[i].src_node;
        int dst = cases[i].dst_dmem;
        
        // Use addresses from memory map
        uint64_t src_addr = TILE0_BASE + src * TILE_STRIDE + DLM1_512_OFFSET;
        uint64_t dst_addr = (dst < 4) ? 
            (DMEM0_512_BASE + dst * DMEM_STRIDE) : 
            (DMEM4_512_BASE + (dst - 4) * DMEM_STRIDE);
        
        // Setup test data via platform structure
        uint8_t *src_ptr = p->nodes[src].dlm1_512_ptr;
        uint8_t *dst_ptr = p->dmems[dst].dmem_ptr;

        /* prepare pattern */
        fill(src_ptr, BYTES, cases[i].seed);
        memset(dst_ptr, 0, BYTES);

        thread_safe_printf("%d. HAL transfer: node_%d.dlm1(0x%lx) -> dmem_%d(0x%lx)\n", 
               i + 1, src, src_addr, dst, dst_addr);
        thread_safe_dump32("[SRC-BEFORE]", src_ptr);
        thread_safe_dump32("[DST-BEFORE]", dst_ptr);

        // Call HAL with addresses (proper interface)
        int result = g_hal.dma_remote_transfer(src_addr, dst_addr, BYTES);

        thread_safe_dump32("[SRC-AFTER ]", src_ptr);
        thread_safe_dump32("[DST-AFTER ]", dst_ptr);

        int ok = (memcmp(src_ptr, dst_ptr, BYTES) == 0);
        pass_cnt += ok;
        thread_safe_printf("HAL result: %d, Verify: %s\n\n", result, ok ? "PASS" : "FAIL");
    }

    thread_safe_printf("[RndDMA] Summary: %d/2 passed\n", pass_cnt);
    return pass_cnt == 2;
}

