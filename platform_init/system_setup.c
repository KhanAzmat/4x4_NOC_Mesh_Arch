#include <stdlib.h>
#include <string.h>
#include <stdio.h>
// #include "c0_master/c0_controller.h"
#include "platform_init/system_setup.h"
#include "platform_init/tile_init.h"
#include "hal_tests/hal_interface.h"
#include "address_manager.h"

// void platform_setup(mesh_platform_t* p)
// {
//     /* allocate global arrays */
//     p->node_count = NODES_COUNT;
//     p->nodes = calloc(NODES_COUNT, sizeof(tile_core_t));
//     p->dmem_count = DMEM_COUNT;
//     p->dmems = calloc(DMEM_COUNT, sizeof(dmem_module_t));

//     platform_init_tiles(p->nodes, p->node_count);

//     hal_use_reference_impl();
//     hal_set_platform(p);
// }

void platform_setup(mesh_platform_t* p)
{
    /* allocate node and dmem structures */
    p->node_count = NUM_TILES;
    p->nodes = calloc(NUM_TILES, sizeof(tile_core_t));
    p->dmem_count = NUM_DMEMS;
    p->dmems = calloc(NUM_DMEMS, sizeof(dmem_module_t));

    // Initialize address manager for HAL/driver use
    address_manager_init(p);
    
    // Setup tiles with both addresses and simulated memory
    for (int i = 0; i < NUM_TILES; i++) {
        // Address space (what HAL sees)
        p->nodes[i].dlm1_512_base_addr = TILE0_BASE + i * TILE_STRIDE + DLM1_512_OFFSET;
        
        // Simulated memory (what tests access)
        p->nodes[i].dlm1_512_ptr = calloc(DLM1_512_SIZE, 1);
        
        // Register memory with address manager
        register_memory_region(p->nodes[i].dlm1_512_base_addr, 
                             p->nodes[i].dlm1_512_ptr, 
                             DLM1_512_SIZE);
        
        // STEP 1: Initialize tile threading structures
        p->nodes[i].id = i;
        p->nodes[i].x = (i % 4);
        p->nodes[i].y = (i / 4) * 2; /* nodes on rows 0 and 2 */
        
        // Thread state will be initialized in platform_start_threads()
        p->nodes[i].running = false;
        p->nodes[i].initialized = false;
        
        // STEP 2: Initialize task execution state
        p->nodes[i].current_task = NULL;
        p->nodes[i].task_pending = false;
        p->nodes[i].idle = true;
        p->nodes[i].tasks_completed = 0;
        p->nodes[i].total_execution_time = 0;
    }

    // Setup DMEMs with real addresses
    uint64_t dmem_bases[] = {
        DMEM0_512_BASE, DMEM1_512_BASE, DMEM2_512_BASE, DMEM3_512_BASE,
        DMEM4_512_BASE, DMEM5_512_BASE, DMEM6_512_BASE, DMEM7_512_BASE
    };

    for (int i = 0; i < NUM_DMEMS; i++) {
        p->dmems[i].id = i;
        p->dmems[i].dmem_base_addr = dmem_bases[i];
        p->dmems[i].dmem_size = DMEM_512_SIZE;
        
        // Allocate simulated memory for DMEM
        p->dmems[i].dmem_ptr = calloc(DMEM_512_SIZE, 1);
        
        // Register memory with address manager
        register_memory_region(p->dmems[i].dmem_base_addr, 
                             p->dmems[i].dmem_ptr, 
                             DMEM_512_SIZE);
    }

    // STEP 1: Initialize platform threading control (main thread = C0 master)
    p->platform_running = false;
    
    // STEP 2: Initialize task coordination system
    p->next_task_id = 1;
    p->active_tasks = 0;
    p->completed_tasks = 0;

    // platform_init_tiles(p->nodes, p->node_count);

    hal_use_reference_impl();
    hal_set_platform(p);
    
    printf("[Platform Setup] Enhanced with threading and task system (Step 2)\n");
}


