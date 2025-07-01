#include <stdlib.h>
#include <string.h>
#include <stdio.h>
// #include "c0_master/c0_controller.h"
#include "platform_init/system_setup.h"
#include "platform_init/tile_init.h"
#include "hal_tests/hal_interface.h"
#include "address_manager.h"
#include "interrupt/plic.h"
#include "tile/tile_dma.h"

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
        // 1. DLM_64 (32 KiB scratchpad memory)
        p->nodes[i].dlm64_base_addr = TILE0_BASE + i * TILE_STRIDE + DLM_64_OFFSET;
        p->nodes[i].dlm64_ptr = calloc(DLM_64_SIZE, 1);
        register_memory_region(p->nodes[i].dlm64_base_addr, 
                             p->nodes[i].dlm64_ptr, 
                             DLM_64_SIZE);
        
        // 2. DLM1_512 (128 KiB buffer memory)
        p->nodes[i].dlm1_512_base_addr = TILE0_BASE + i * TILE_STRIDE + DLM1_512_OFFSET;
        p->nodes[i].dlm1_512_ptr = calloc(DLM1_512_SIZE, 1);
        register_memory_region(p->nodes[i].dlm1_512_base_addr, 
                             p->nodes[i].dlm1_512_ptr, 
                             DLM1_512_SIZE);
        
        // 3. DMA Registers (4 KiB control block)
        p->nodes[i].dma_reg_base_addr = TILE0_BASE + i * TILE_STRIDE + DMA_REG_OFFSET;
        p->nodes[i].dma_regs_ptr = calloc(0x1000, 1);  // 4 KiB for DMA registers
        register_memory_region(p->nodes[i].dma_reg_base_addr, 
                             p->nodes[i].dma_regs_ptr, 
                             0x1000);
        
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
        
        // STEP 3: Initialize DMAC512 for this tile
        int dmac_init_result = dma_tile_init(i);
        if (dmac_init_result == 0) {
            p->nodes[i].dmac512_initialized = true;
        } else {
            p->nodes[i].dmac512_initialized = false;
            printf("Warning: Failed to initialize DMAC512 for tile %d\n", i);
        }
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
    

    // Initialize PLIC memory regions
    uint8_t* plic_c0c1_memory = calloc(PLIC_SIZE, 1);
    uint8_t* plic_nxy_memory = calloc(PLIC_SIZE, 1);
    
    // CRITICAL FIX: Assign allocated memory to platform structure
    p->plic_instances[0].c0c1_ptr = plic_c0c1_memory;
    p->plic_instances[0].nxy_ptr = plic_nxy_memory;
    p->plic_instances[0].c0c1_base = PLIC_0_C0C1_BASE;
    p->plic_instances[0].nxy_base = PLIC_0_NXY_BASE;
    
    register_memory_region(PLIC_0_C0C1_BASE, plic_c0c1_memory, PLIC_SIZE);
    register_memory_region(PLIC_0_NXY_BASE, plic_nxy_memory, PLIC_SIZE);
    
    // Initialize PLIC basic structures for each hart
    for (int hart_id = 0; hart_id < NUM_TILES; hart_id++) {
        plic_init_for_this_hart(hart_id);
    }
    
    // Setup bidirectional interrupt capabilities (replaces hardcoded setup)
    PLIC_setup_bidirectional_interrupts();
    
    printf("[Platform Setup] Enhanced PLIC integration complete with bidirectional support\n");
    
    printf("[Platform Setup] (Step 2)\n");

}


