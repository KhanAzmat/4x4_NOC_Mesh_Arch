#define _GNU_SOURCE
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include "c0_controller.h"
#include "hal_tests/test_framework.h"

// STEP 1: Basic tile processor main loop
void* tile_processor_main(void* arg)
{
    tile_core_t* tile = (tile_core_t*)arg;
    
    printf("[Tile %d] Starting processor thread...\n", tile->id);
    
    // Initialize tile state
    pthread_mutex_lock(&tile->state_lock);
    tile->running = true;
    tile->initialized = true;
    pthread_mutex_unlock(&tile->state_lock);
    
    // Basic processor loop - just idle for now
    while (tile->running) {
        // STEP 1: Simple idle loop
        // In later steps, this will handle tasks from C0 master
        usleep(1000); // 1ms idle sleep
        
        // Check if we should stop
        pthread_mutex_lock(&tile->state_lock);
        bool should_run = tile->running;
        pthread_mutex_unlock(&tile->state_lock);
        
        if (!should_run) break;
    }
    
    printf("[Tile %d] Processor thread stopping...\n", tile->id);
    return NULL;
}

// STEP 1: Start tile threads (main thread = C0 master)
int platform_start_tile_threads(mesh_platform_t* p)
{
    printf("[C0 Master] Starting tile processor threads...\n");
    
    // Initialize platform state
    pthread_mutex_init(&p->platform_lock, NULL);
    p->platform_running = true;
    
    // Initialize all tile threads
    for (int i = 0; i < p->node_count; i++) {
        tile_core_t* tile = &p->nodes[i];
        
        // Initialize thread state
        pthread_mutex_init(&tile->state_lock, NULL);
        tile->running = false;
        tile->initialized = false;
        
        // Create tile thread
        if (pthread_create(&tile->thread_id, NULL, tile_processor_main, tile) != 0) {
            printf("[C0 Master] ERROR: Failed to create thread for tile %d\n", i);
            return -1;
        }
    }
    
    // Wait for all tiles to initialize
    printf("[C0 Master] Waiting for tile threads to initialize...\n");
    
    // Wait for all tiles to initialize
    for (int i = 0; i < p->node_count; i++) {
        while (!p->nodes[i].initialized) {
            usleep(1000);
        }
    }
    
    printf("[C0 Master] All tile threads initialized successfully!\n");
    return 0;
}

// STEP 1: Stop tile threads
int platform_stop_tile_threads(mesh_platform_t* p)
{
    printf("[C0 Master] Stopping tile processor threads...\n");
    
    // Signal all tile threads to stop
    for (int i = 0; i < p->node_count; i++) {
        pthread_mutex_lock(&p->nodes[i].state_lock);
        p->nodes[i].running = false;
        pthread_mutex_unlock(&p->nodes[i].state_lock);
    }
    
    // Wait for all tile threads to finish
    for (int i = 0; i < p->node_count; i++) {
        pthread_join(p->nodes[i].thread_id, NULL);
    }
    
    // Clean up mutexes
    for (int i = 0; i < p->node_count; i++) {
        pthread_mutex_destroy(&p->nodes[i].state_lock);
    }
    pthread_mutex_destroy(&p->platform_lock);
    
    p->platform_running = false;
    printf("[C0 Master] All tile threads stopped successfully!\n");
    return 0;
}

// STEP 1: C0 master supervision function (executed by main thread)
void c0_master_supervise_tiles(mesh_platform_t* p)
{
    printf("[C0 Master] Supervising tile processors...\n");
    
    // Simple supervision - in later steps this will coordinate tasks
    int supervision_cycles = 0;
    
    while (p->platform_running && supervision_cycles < 5) {
        // Check tile status
        int active_tiles = 0;
        for (int i = 0; i < p->node_count; i++) {
            pthread_mutex_lock(&p->nodes[i].state_lock);
            if (p->nodes[i].running) {
                active_tiles++;
            }
            pthread_mutex_unlock(&p->nodes[i].state_lock);
        }
        
        printf("[C0 Master] Supervision cycle %d: %d tiles active\n", 
               supervision_cycles + 1, active_tiles);
        
        usleep(200000); // 200ms supervision interval
        supervision_cycles++;
    }
    
    printf("[C0 Master] Supervision complete\n");
}

// Enhanced test runner with main thread as C0 master
void c0_run_test_suite(mesh_platform_t* platform)
{
    printf("\n== Mesh‑NoC HAL Validation (Main Thread = C0 Master) ==\n");
    
    // STEP 1: Start tile processor threads (main thread becomes C0 master)
    if (platform_start_tile_threads(platform) == 0) {
        printf("[C0 Master] Platform running with tile processors in parallel!\n");
        
        // C0 master supervises the platform
        c0_master_supervise_tiles(platform);
        
        // Run the existing test suite (C0 master coordinates)
        printf("[C0 Master] Executing test suite...\n");
        run_all_tests(platform);
        
        // STEP 1: Stop tile processor threads
        platform_stop_tile_threads(platform);
    } else {
        printf("[C0 Master] ERROR: Failed to start tile threads, running in single-threaded mode\n");
        run_all_tests(platform);
    }
}
