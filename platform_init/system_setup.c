#include <stdlib.h>
#include <string.h>
#include <stdio.h>
// #include "c0_master/c0_controller.h"
#include "platform_init/system_setup.h"
#include "platform_init/tile_init.h"
#include "hal_tests/hal_interface.h"
#include "address_manager.h"
#include "hal/INT/plic.h"
#include "dmac512_hardware_monitor.h"

// Global platform pointer for hook access
mesh_platform_t* global_platform_ptr = NULL;

// Using post-HAL monitoring instead of register write interception
// HAL/Driver code executes normally, then we check register state after completion

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
    if (!p) {
        return;
    }
    
    // Set global platform pointer for hook access
    global_platform_ptr = p;

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
        
        // 4. DMAC512 HAL initialization (Phase 1)
        // Cast existing DMA register space to DMAC512 registers
        p->nodes[i].dmac512_regs = (DMAC512_RegDef*)p->nodes[i].dma_regs_ptr;
        
        // Initialize DMAC512 HAL handle with memory-mapped address
        HAL_DMAC512InitHandle(&p->nodes[i].dmac512_handle, 
                              p->nodes[i].dmac512_regs);
        
        // Initialize DMAC512 handle configuration with defaults
        p->nodes[i].dmac512_handle.Init.DmacMode = DMAC512_NORMAL_MODE;
        p->nodes[i].dmac512_handle.Init.dob_beat = DMAC512_AXI_TRANS_4;
        p->nodes[i].dmac512_handle.Init.dfb_beat = DMAC512_AXI_TRANS_4;
        p->nodes[i].dmac512_handle.Init.SrcAddr = 0;
        p->nodes[i].dmac512_handle.Init.DstAddr = 0;
        p->nodes[i].dmac512_handle.Init.XferCount = 0;
        
        // Initialize DMAC512 transfer state
        p->nodes[i].dmac512_busy = false;
        p->nodes[i].dmac512_transfer_id = 0;
        
        // 5. PLIC HAL initialization - CORRECTED to match HAL expectations
        // Map 8 platform tiles to PLIC hart ranges per HAL architecture:
        // PLIC_TARGET_BASE[3] = { 0, 2, 18 } and PLIC_TARGET_COUNT[3] = { 2, 16, 16 }
        // Hart 0-1: PLIC_0, Hart 2-17: PLIC_1, Hart 18-24: PLIC_2
        
        // Hart ID maps directly to tile ID for our 8-tile platform (0-7)
        p->nodes[i].plic_hart_id = i;
        
        if (i < 2) {
            // Tiles 0-1 → Harts 0-1 → PLIC_0 (targets 0-1)
            p->nodes[i].plic_instance = 0;
            p->nodes[i].plic_target_id = i;  // Target 0 or 1 within PLIC_0
        } else {
            // Tiles 2-7 → Harts 2-7 → PLIC_1 (targets 0-5)
            p->nodes[i].plic_instance = 1;
            p->nodes[i].plic_target_id = i - 2;  // Targets 0-5 within PLIC_1
        }
        
        // NOTE: Harts 8-24 are unused in our 8-tile platform
        // PLIC_2 (harts 18-24) is not used by platform tiles
        
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

    // 6. PLIC Register Memory Allocation - Map PLIC hardware addresses to simulated memory
    // The PLIC HAL accesses real memory addresses, so we need to allocate and map them
    
    // PLIC base addresses from HAL (6 different memory regions)
    uint64_t plic_bases[] = {
        PLIC_0_C0C1_BASE, PLIC_0_NXY_BASE,   // PLIC instance 0 
        PLIC_1_C0C1_BASE, PLIC_1_NXY_BASE,   // PLIC instance 1
        PLIC_2_C0C1_BASE, PLIC_2_NXY_BASE    // PLIC instance 2 (unused but allocated)
    };
    
    // Allocate simulated PLIC register memory for each base address
    void* plic_memories[6];  // Store pointers for debugging
    for (int i = 0; i < 6; i++) {
        plic_memories[i] = calloc(sizeof(PLIC_RegDef), 1);  // One PLIC_RegDef per base
        if (!plic_memories[i]) {
            printf("[PLIC-HAL] ERROR: Failed to allocate memory for PLIC base 0x%lx\n", plic_bases[i]);
            exit(1);
        }
        
        // Register the PLIC memory region with address manager
        register_memory_region(plic_bases[i], plic_memories[i], sizeof(PLIC_RegDef));
        printf("[PLIC-HAL] Allocated PLIC registers at 0x%lx (size %lu) -> ptr %p\n", 
               plic_bases[i], sizeof(PLIC_RegDef), plic_memories[i]);
    }
    
    // 7. PLIC HAL Global Initialization - Initialize PLIC_INST[] array and configure interrupts
    // This must be done after all tiles are configured to set up shared PLIC instances
    
    // Initialize PLIC instances per HAL requirements
    // The HAL expects plic_init_for_this_hart() to be called for each hart
    printf("[PLIC-HAL] Initializing PLIC instances...\n");
    for (int i = 0; i < NUM_TILES; i++) {
        printf("[PLIC-HAL] Calling plic_init_for_this_hart(%d)\n", p->nodes[i].plic_hart_id);
        plic_init_for_this_hart(p->nodes[i].plic_hart_id);
    }
    
    // CRITICAL FIX: Convert PLIC_INST hardware addresses to simulation pointers
    // The HAL sets PLIC_INST to hardware addresses, but in simulation we need actual pointers
    extern volatile PLIC_RegDef *PLIC_INST[3];
    printf("[PLIC-HAL] Converting PLIC_INST hardware addresses to simulation pointers...\n");
    for (int i = 0; i < 3; i++) {
        uint64_t hw_addr = (uint64_t)PLIC_INST[i];
        void* sim_ptr = addr_to_ptr(hw_addr);
        if (sim_ptr) {
            PLIC_INST[i] = (volatile PLIC_RegDef*)sim_ptr;
            printf("[PLIC-HAL]   PLIC_INST[%d]: 0x%lx -> %p (SUCCESS)\n", i, hw_addr, sim_ptr);
        } else {
            printf("[PLIC-HAL]   PLIC_INST[%d]: 0x%lx -> (nil) (ERROR)\n", i, hw_addr);
            // Manual mapping as fallback
            if (hw_addr == PLIC_0_NXY_BASE) {
                PLIC_INST[i] = (volatile PLIC_RegDef*)plic_memories[1];  // PLIC_0_NXY_BASE is index 1
                printf("[PLIC-HAL]   Manual override: PLIC_INST[%d] -> %p\n", i, plic_memories[1]);
            } else if (hw_addr == PLIC_1_NXY_BASE) {
                PLIC_INST[i] = (volatile PLIC_RegDef*)plic_memories[3];  // PLIC_1_NXY_BASE is index 3
                printf("[PLIC-HAL]   Manual override: PLIC_INST[%d] -> %p\n", i, plic_memories[3]);
            } else if (hw_addr == PLIC_2_NXY_BASE) {
                PLIC_INST[i] = (volatile PLIC_RegDef*)plic_memories[5];  // PLIC_2_NXY_BASE is index 5
                printf("[PLIC-HAL]   Manual override: PLIC_INST[%d] -> %p\n", i, plic_memories[5]);
            }
        }
    }
    
    // Configure interrupts for each hart using PLIC HAL
    for (int i = 0; i < NUM_TILES; i++) {
        uint32_t hart_id = p->nodes[i].plic_hart_id;
        
        // Enable all interrupt sources for this hart
        PLIC_enable_interrupt(IRQ_MESH_NODE, hart_id);  // Task completion
        PLIC_enable_interrupt(IRQ_DMA512, hart_id);     // DMA completion
        PLIC_enable_interrupt(IRQ_GPIO, hart_id);       // Error signals
        PLIC_enable_interrupt(IRQ_PIT, hart_id);        // Timer interrupts
        PLIC_enable_interrupt(IRQ_SPI1, hart_id);       // Resource requests
        PLIC_enable_interrupt(IRQ_RTC_ALARM, hart_id);  // Shutdown requests
        
        // Set interrupt priorities using PLIC HAL
        PLIC_set_priority(IRQ_GPIO, hart_id, 7);        // Errors - highest priority
        PLIC_set_priority(IRQ_RTC_ALARM, hart_id, 6);   // Shutdown - high priority
        PLIC_set_priority(IRQ_DMA512, hart_id, 5);      // DMA - high priority
        PLIC_set_priority(IRQ_SPI1, hart_id, 4);        // Resources - medium priority
        PLIC_set_priority(IRQ_MESH_NODE, hart_id, 3);   // Tasks - normal priority
        PLIC_set_priority(IRQ_PIT, hart_id, 2);         // Timer - low priority
        
        // Set interrupt threshold (interrupts with priority > threshold will be delivered)
        PLIC_set_threshold(hart_id, 0);  // Accept all interrupts (priority > 0)
    }
    
    // The HAL now has PLIC_INST[0], PLIC_INST[1] pointing to the correct base addresses
    // PLIC_INST[2] remains unused since we don't have harts 18-24
    
    // 8. Initialize PLIC statistics (optional)
    platform_init_plic_stats(p);
    
    printf("[PLIC-HAL] Initialized PLIC instances for harts 0-%d\n", NUM_TILES-1);
    printf("[PLIC-HAL] PLIC_0: harts 0-1, PLIC_1: harts 2-7, PLIC_2: unused\n");
    printf("[PLIC-HAL] Enabled interrupts: MESH_NODE, DMA512, GPIO, PIT, SPI1, RTC_ALARM for all harts\n");
    // STEP 1: Initialize platform threading control (main thread = C0 master)
    p->platform_running = false;
    
    // STEP 2: Initialize task coordination system
    p->next_task_id = 1;
    p->active_tasks = 0;
    p->completed_tasks = 0;

    // platform_init_tiles(p->nodes, p->node_count);

    hal_use_reference_impl();
    hal_set_platform(p);
    
    printf("[Platform Setup] DMAC512 + PLIC Integration Complete\n");
    printf("  - %d tiles initialized with DMAC512 HAL handles\n", NUM_TILES);
    printf("  - %d tiles initialized with PLIC HAL (instances 0,1,2)\n", NUM_TILES);
    printf("  - DMAC512 registers mapped to memory addresses\n");
    printf("  - PLIC interrupts configured using original HAL functions\n");
}

// Platform now intercepts register writes instead of function calls
// HAL/Driver code executes normally and writes to registers
// Our hook detects the register writes and triggers hardware emulation


