// ===================== File: hal_tests/dmem_tests.c =====================
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdarg.h>
#include <time.h>
#include "dmem_tests.h"
#include "hal_tests/hal_interface.h"
#include "generated/mem_map.h"

/* -------------------------------------------------------------------------- */
/*                               Thread‑safe I/O                              */
/* -------------------------------------------------------------------------- */
static pthread_mutex_t print_mutex = PTHREAD_MUTEX_INITIALIZER;

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

static void thread_safe_banner(const char *msg)
{
    pthread_mutex_lock(&print_mutex);

    char complete_banner[1000];
    int msg_len             = (int)strlen(msg);
    int total_width         = 82;
    int padding             = total_width - msg_len;

    snprintf(complete_banner,
             sizeof(complete_banner),
             "\n"
             "╔═══════════════════════════════════════════════════════════════════════════════════╗\n"
             "║ \033[1;32m%s\033[0m%*s║\n"
             "╚═══════════════════════════════════════════════════════════════════════════════════╝\n"
             "\n",
             msg,
             padding,
             "");

    fputs(complete_banner, stdout);
    fflush(stdout);
    pthread_mutex_unlock(&print_mutex);
}

static void thread_safe_dump32(const char* tag, const uint8_t* buf)
{
    pthread_mutex_lock(&print_mutex);
    printf("%s 0x", tag);
    for (int i = 0; i < 32; i++) printf("%02X", buf[i]);
    printf(" ...\n");
    fflush(stdout);
    pthread_mutex_unlock(&print_mutex);
}

/* -------------------------------------------------------------------------- */
/*                               Test helpers                                 */
/* -------------------------------------------------------------------------- */
static int verify_buffer_equal(const uint8_t* a, const uint8_t* b, size_t bytes)
{
    return (memcmp(a, b, bytes) == 0);
}

static double timespec_diff_sec(const struct timespec* start, const struct timespec* end)
{
    return (double)(end->tv_sec - start->tv_sec) +
           (double)(end->tv_nsec - start->tv_nsec) / 1e9;
}

/* -------------------------------------------------------------------------- */
/*                             1. Basic Functionality                         */
/* -------------------------------------------------------------------------- */
int test_dmem_basic_functionality(mesh_platform_t* p)
{
    (void)p; /* p is unused for these HAL‑only checks */
    const size_t bytes = 256;
    uint64_t src_addr  = DMEM0_512_BASE;
    uint64_t dst_addr  = DMEM1_512_BASE;

    thread_safe_banner("DMEM Basic Functionality");

    thread_safe_printf("[DEBUG] Starting memory_fill...\n");
    int fill_result = g_hal.memory_fill(src_addr, 0xAA, bytes);
    thread_safe_printf("[DEBUG] memory_fill result: %d\n", fill_result);
    
    thread_safe_printf("[DEBUG] Starting memory_set...\n");
    int set_result = g_hal.memory_set(dst_addr, 0, bytes);
    thread_safe_printf("[DEBUG] memory_set result: %d\n", set_result);

    thread_safe_printf("[DEBUG] Starting dmem_to_dmem_transfer...\n");
    int result = g_hal.dmem_to_dmem_transfer(src_addr, dst_addr, bytes);
    thread_safe_printf("[DEBUG] dmem_to_dmem_transfer result: %d\n", result);

    uint8_t src_verify[bytes];
    uint8_t dst_verify[bytes];
    
    thread_safe_printf("[DEBUG] Starting memory_read src...\n");
    int read_src_result = g_hal.memory_read(src_addr, src_verify, bytes);
    thread_safe_printf("[DEBUG] memory_read src result: %d\n", read_src_result);
    
    thread_safe_printf("[DEBUG] Starting memory_read dst...\n");
    int read_dst_result = g_hal.memory_read(dst_addr, dst_verify, bytes);
    thread_safe_printf("[DEBUG] memory_read dst result: %d\n", read_dst_result);

    int buffers_equal = verify_buffer_equal(src_verify, dst_verify, bytes);
    thread_safe_printf("[DEBUG] buffers_equal: %d\n", buffers_equal);

    int ok = (result == 0) && buffers_equal;
    thread_safe_printf("[DEBUG] Final ok: %d (result==0: %d, buffers_equal: %d)\n", ok, (result == 0), buffers_equal);

    thread_safe_printf("[Test] DMEM Basic Functionality: %s\n", ok ? "PASS" : "FAIL");
    return ok;
}

/* -------------------------------------------------------------------------- */
/*                              2. Large Transfers                            */
/* -------------------------------------------------------------------------- */
int test_dmem_large_transfers(mesh_platform_t* p)
{
    (void)p;
    const size_t bytes = 262144; /* 256 KiB */
    uint64_t src_addr  = DMEM0_512_BASE;
    uint64_t dst_addr  = DMEM1_512_BASE;

    thread_safe_banner("DMEM Large Transfers");

    g_hal.memory_fill(src_addr, 0x55, bytes);
    g_hal.memory_set(dst_addr, 0, bytes);

    int result = g_hal.dmem_to_dmem_transfer(src_addr, dst_addr, bytes);

    uint8_t* src_verify = (uint8_t*)malloc(bytes);
    uint8_t* dst_verify = (uint8_t*)malloc(bytes);

    g_hal.memory_read(src_addr, src_verify, bytes);
    g_hal.memory_read(dst_addr, dst_verify, bytes);

    int ok = (result == 0) && verify_buffer_equal(src_verify, dst_verify, bytes);

    thread_safe_printf("[Test] DMEM Large Transfers: %s\n", ok ? "PASS" : "FAIL");

    free(src_verify);
    free(dst_verify);
    return ok;
}

/* -------------------------------------------------------------------------- */
/*                           3. Address Validation                            */
/* -------------------------------------------------------------------------- */
int test_dmem_address_validation(mesh_platform_t* p)
{
    (void)p;
    thread_safe_banner("DMEM Address Validation");

    int r1 = g_hal.dmem_to_dmem_transfer(DMEM0_512_BASE, DMEM1_512_BASE, 1024);
    int r2 = g_hal.dmem_to_dmem_transfer(DMEM0_512_BASE, DMEM7_512_BASE, 1024);

    int s0 = g_hal.get_dmem_status(DMEM0_512_BASE);
    int s1 = g_hal.get_dmem_status(DMEM1_512_BASE);
    int s7 = g_hal.get_dmem_status(DMEM7_512_BASE);

    int ok = (r1 == 0) && (r2 == 0) && (s0 == 0) && (s1 == 0) && (s7 == 0);

    thread_safe_printf("[Test] DMEM Address Validation: %s\n", ok ? "PASS" : "FAIL");
    return ok;
}

/* -------------------------------------------------------------------------- */
/*                            4. Data Integrity                               */
/* -------------------------------------------------------------------------- */
int test_dmem_data_integrity(mesh_platform_t* p)
{
    (void)p;
    thread_safe_banner("DMEM Data Integrity");

    const size_t bytes = 1024;
    uint8_t pattern[bytes];
    for (size_t i = 0; i < bytes; ++i) pattern[i] = (uint8_t)(i % 256);

    g_hal.memory_write(DMEM0_512_BASE, pattern, bytes);
    g_hal.memory_set(DMEM1_512_BASE, 0, bytes);

    int result = g_hal.dmem_to_dmem_transfer(DMEM0_512_BASE, DMEM1_512_BASE, bytes);

    uint8_t dst_verify[bytes];
    g_hal.memory_read(DMEM1_512_BASE, dst_verify, bytes);

    int ok = (result == 0) && verify_buffer_equal(pattern, dst_verify, bytes);

    thread_safe_printf("[Test] DMEM Data Integrity: %s\n", ok ? "PASS" : "FAIL");
    return ok;
}

/* -------------------------------------------------------------------------- */
/*                           5. Concurrent Access                             */
/* -------------------------------------------------------------------------- */
struct transfer_args {
    uint64_t src;
    uint64_t dst;
    size_t   bytes;
    int      result;
};

static void* transfer_thread(void* arg)
{
    struct transfer_args* a = (struct transfer_args*)arg;
    a->result               = g_hal.dmem_to_dmem_transfer(a->src, a->dst, a->bytes);
    return NULL;
}

int test_dmem_concurrent_access(mesh_platform_t* p)
{
    (void)p;
    thread_safe_banner("DMEM Concurrent Access");

    const size_t bytes = 1024;

    g_hal.memory_fill(DMEM0_512_BASE, 0xCC, bytes);
    g_hal.memory_fill(DMEM2_512_BASE, 0x33, bytes);
    g_hal.memory_set(DMEM1_512_BASE, 0, bytes);
    g_hal.memory_set(DMEM3_512_BASE, 0, bytes);

    struct transfer_args a1 = {DMEM0_512_BASE, DMEM1_512_BASE, bytes, -1};
    struct transfer_args a2 = {DMEM2_512_BASE, DMEM3_512_BASE, bytes, -1};

    pthread_t t1, t2;
    pthread_create(&t1, NULL, transfer_thread, &a1);
    pthread_create(&t2, NULL, transfer_thread, &a2);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    uint8_t v1_src[bytes], v1_dst[bytes];
    uint8_t v2_src[bytes], v2_dst[bytes];

    g_hal.memory_read(DMEM0_512_BASE, v1_src, bytes);
    g_hal.memory_read(DMEM1_512_BASE, v1_dst, bytes);
    g_hal.memory_read(DMEM2_512_BASE, v2_src, bytes);
    g_hal.memory_read(DMEM3_512_BASE, v2_dst, bytes);

    int ok = (a1.result == 0) && (a2.result == 0) &&
             verify_buffer_equal(v1_src, v1_dst, bytes) &&
             verify_buffer_equal(v2_src, v2_dst, bytes);

    thread_safe_printf("[Test] DMEM Concurrent Access: %s\n", ok ? "PASS" : "FAIL");
    return ok;
}

/* -------------------------------------------------------------------------- */
/*                          6. Boundary Conditions                            */
/* -------------------------------------------------------------------------- */
int test_dmem_boundary_conditions(mesh_platform_t* p)
{
    (void)p;
    thread_safe_banner("DMEM Boundary Conditions");

    int r_first_byte = g_hal.dmem_to_dmem_transfer(DMEM0_512_BASE, DMEM1_512_BASE, 1);

    uint64_t near_end_src = DMEM0_512_BASE + DMEM_512_SIZE - 256;
    int r_last_chunk      = g_hal.dmem_to_dmem_transfer(near_end_src, DMEM1_512_BASE, 256);

    int ok = (r_first_byte == 0) && (r_last_chunk == 0);

    thread_safe_printf("[Test] DMEM Boundary Conditions: %s\n", ok ? "PASS" : "FAIL");
    return ok;
}

/* -------------------------------------------------------------------------- */
/*                           7. Error Handling                                */
/* -------------------------------------------------------------------------- */
int test_dmem_error_handling(mesh_platform_t* p)
{
    (void)p;
    thread_safe_banner("DMEM Error Handling");

    int r_zero_size = g_hal.dmem_to_dmem_transfer(DMEM0_512_BASE, DMEM1_512_BASE, 0);

    uint8_t* null_ptr = NULL;
    int r_null_read   = g_hal.memory_read(DMEM0_512_BASE, null_ptr, 256);

    int ok = (r_zero_size != 0) && (r_null_read != 0);

    thread_safe_printf("[Test] DMEM Error Handling: %s\n", ok ? "PASS" : "FAIL");
    return ok;
}

/* -------------------------------------------------------------------------- */
/*                          8. Performance (Basic)                            */
/* -------------------------------------------------------------------------- */
int test_dmem_performance_basic(mesh_platform_t* p)
{
    (void)p;
    thread_safe_banner("DMEM Performance Basic");

    const size_t bytes = 65536; /* 64 KiB */

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    int result = g_hal.dmem_to_dmem_transfer(DMEM0_512_BASE, DMEM1_512_BASE, bytes);
    clock_gettime(CLOCK_MONOTONIC, &end);

    double elapsed = timespec_diff_sec(&start, &end);
    double bps     = (bytes / elapsed);

    thread_safe_printf("    Bytes transferred: %zu\n", bytes);
    thread_safe_printf("    Elapsed time    : %.6f s\n", elapsed);
    thread_safe_printf("    Throughput      : %.2f B/s\n", bps);

    int ok = (result == 0);
    thread_safe_printf("[Test] DMEM Performance Basic: %s\n", ok ? "PASS" : "FAIL");
    return ok;
}

/* -------------------------------------------------------------------------- */
/*                       9. Cross‑Module Transfers                             */
/* -------------------------------------------------------------------------- */
int test_dmem_cross_module_transfers(mesh_platform_t* p)
{
    (void)p;
    thread_safe_banner("DMEM Cross‑Module Transfers");

    /* Pre‑fill distinct patterns so we can sanity‑check later */
    g_hal.memory_fill(DMEM0_512_BASE, 0x11, 256);
    g_hal.memory_fill(DMEM1_512_BASE, 0x22, 512);
    g_hal.memory_fill(DMEM2_512_BASE, 0x33, 1024);

    int r1 = g_hal.dmem_to_dmem_transfer(DMEM0_512_BASE, DMEM3_512_BASE, 256);
    int r2 = g_hal.dmem_to_dmem_transfer(DMEM1_512_BASE, DMEM6_512_BASE, 512);
    int r3 = g_hal.dmem_to_dmem_transfer(DMEM2_512_BASE, DMEM7_512_BASE, 1024);

    uint8_t v_src[1024];
    uint8_t v_dst[1024];

    g_hal.memory_read(DMEM0_512_BASE, v_src, 256);
    g_hal.memory_read(DMEM3_512_BASE, v_dst, 256);
    int ok1 = verify_buffer_equal(v_src, v_dst, 256);

    g_hal.memory_read(DMEM1_512_BASE, v_src, 512);
    g_hal.memory_read(DMEM6_512_BASE, v_dst, 512);
    int ok2 = verify_buffer_equal(v_src, v_dst, 512);

    g_hal.memory_read(DMEM2_512_BASE, v_src, 1024);
    g_hal.memory_read(DMEM7_512_BASE, v_dst, 1024);
    int ok3 = verify_buffer_equal(v_src, v_dst, 1024);

    int ok = (r1 == 0) && (r2 == 0) && (r3 == 0) && ok1 && ok2 && ok3;

    thread_safe_printf("[Test] DMEM Cross‑Module Transfers: %s\n", ok ? "PASS" : "FAIL");
    return ok;
}

/* -------------------------------------------------------------------------- */
/*                           10. Alignment Testing                            */
/* -------------------------------------------------------------------------- */
static int run_alignment_case(uint64_t src, uint64_t dst, size_t bytes)
{
    g_hal.memory_fill(src, 0x5A, bytes);
    g_hal.memory_set(dst, 0, bytes);

    int r         = g_hal.dmem_to_dmem_transfer(src, dst, bytes);
    uint8_t* sbuf = (uint8_t*)malloc(bytes);
    uint8_t* dbuf = (uint8_t*)malloc(bytes);

    g_hal.memory_read(src, sbuf, bytes);
    g_hal.memory_read(dst, dbuf, bytes);
    int ok = (r == 0) && verify_buffer_equal(sbuf, dbuf, bytes);

    free(sbuf);
    free(dbuf);
    return ok;
}

int test_dmem_alignment_testing(mesh_platform_t* p)
{
    (void)p;
    thread_safe_banner("DMEM Alignment Testing");

    int ok1 = run_alignment_case(DMEM0_512_BASE + 1, DMEM1_512_BASE + 1, 255);
    int ok2 = run_alignment_case(DMEM0_512_BASE + 3, DMEM1_512_BASE + 3, 253);
    int ok3 = run_alignment_case(DMEM0_512_BASE + 7, DMEM1_512_BASE + 7, 249);

    int ok = ok1 && ok2 && ok3;
    thread_safe_printf("[Test] DMEM Alignment Testing: %s\n", ok ? "PASS" : "FAIL");
    return ok;
}