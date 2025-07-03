#include <stdio.h>
#include <stdint.h>
#include "plic.h"
#include "c0_master/c0_controller.h"

// ============================================================================
// DEVICE-SIDE INTERRUPT SOURCES (Hardware Perspective)
// ============================================================================
// These functions represent peripheral devices/controllers that assert
// interrupt lines to the PLIC, following the real SoC flow:
// Device → Gateway → PLIC → Hart

// Task completion device (simulates task completion hardware)
void device_task_completion_interrupt(uint32_t completing_hart_id, uint32_t task_id) {
    // This represents a hardware task completion unit asserting its interrupt line
    printf("[Device-TaskComp] Hart %d completed task %d -> asserting IRQ_MESH_NODE\n", 
           completing_hart_id, task_id);
    
    // Device asserts interrupt line → PLIC gateway sets pending bit
    volatile PLIC_RegDef *plic;
    uint32_t tgt_local;
    plic_select(completing_hart_id, &plic, &tgt_local);
    PLIC_N_source_pending_write((PLIC_RegDef*)plic, IRQ_MESH_NODE);
}

// DMA completion device (already implemented in address_manager.c)
void device_dma_completion_interrupt(uint32_t source_hart_id, uint32_t transfer_id) {
    // This represents DMAC512 hardware asserting its interrupt line
    printf("[Device-DMAC512] Hart %d completed DMA transfer %d -> asserting IRQ_DMA512\n", 
           source_hart_id, transfer_id);
    
    // Device asserts interrupt line → PLIC gateway sets pending bit
    volatile PLIC_RegDef *plic;
    uint32_t tgt_local;
    plic_select(source_hart_id, &plic, &tgt_local);
    PLIC_N_source_pending_write((PLIC_RegDef*)plic, IRQ_DMA512);
}

// Error detection device (simulates error detection hardware)
void device_error_interrupt(uint32_t source_hart_id, uint32_t error_code) {
    // This represents error detection hardware (watchdog, fault unit, etc.)
    printf("[Device-Error] Hart %d detected error %d -> asserting IRQ_GPIO\n", 
           source_hart_id, error_code);
    
    // Device asserts interrupt line → PLIC gateway sets pending bit
    volatile PLIC_RegDef *plic;
    uint32_t tgt_local;
    plic_select(source_hart_id, &plic, &tgt_local);
    PLIC_N_source_pending_write((PLIC_RegDef*)plic, IRQ_GPIO);
}

// Timer device (simulates timer/PIT hardware)
void device_timer_interrupt(uint32_t timer_id) {
    // This represents timer hardware asserting its interrupt line
    printf("[Device-Timer] Timer %d expired -> asserting IRQ_PIT\n", timer_id);
    
    // Device asserts interrupt line → PLIC gateway sets pending bit
    volatile PLIC_RegDef *plic;
    uint32_t tgt_local;
    // Timer is global, use hart 0 as representative
    plic_select(0, &plic, &tgt_local);
    PLIC_N_source_pending_write((PLIC_RegDef*)plic, IRQ_PIT);
}

// Resource management device (simulates resource controller hardware)
void device_resource_request_interrupt(uint32_t requesting_hart_id, uint32_t resource_id) {
    // This represents resource management hardware
    printf("[Device-Resource] Hart %d requesting resource %d -> asserting IRQ_SPI1\n", 
           requesting_hart_id, resource_id);
    
    // Device asserts interrupt line → PLIC gateway sets pending bit  
    volatile PLIC_RegDef *plic;
    uint32_t tgt_local;
    plic_select(requesting_hart_id, &plic, &tgt_local);
    PLIC_N_source_pending_write((PLIC_RegDef*)plic, IRQ_SPI1);
}

// Shutdown controller device (simulates power management hardware)
void device_shutdown_request_interrupt(uint32_t requesting_hart_id) {
    // This represents power management hardware
    printf("[Device-Shutdown] Hart %d requesting shutdown -> asserting IRQ_RTC_ALARM\n", 
           requesting_hart_id);
    
    // Device asserts interrupt line → PLIC gateway sets pending bit
    volatile PLIC_RegDef *plic;
    uint32_t tgt_local;
    plic_select(requesting_hart_id, &plic, &tgt_local);
    PLIC_N_source_pending_write((PLIC_RegDef*)plic, IRQ_RTC_ALARM);
}

// ============================================================================
// HART INTERRUPT PROCESSING (CPU Perspective)
// ============================================================================
// These functions handle interrupts that the PLIC delivers to hart cores

// Process all pending interrupts for a specific hart
int plic_process_hart_interrupts(uint32_t hart_id) {
    int interrupts_handled = 0;
    irq_source_id_t claimed_irq;
    
    // Claim interrupts until none are pending
    volatile PLIC_RegDef *plic;
    uint32_t tgt_local;
    plic_select(hart_id, &plic, &tgt_local);
    
    while ((claimed_irq = PLIC_M_TAR_claim_read((PLIC_RegDef*)plic, tgt_local)) != 0) {
        printf("[Hart-%d-PLIC] Claimed interrupt %s (%d)\n", 
               hart_id, get_plic_irq_name(claimed_irq), claimed_irq);
        
        // Handle the interrupt based on source
        switch (claimed_irq) {
            case IRQ_MESH_NODE:
                printf("[Hart-%d-Handler] Processing task completion interrupt\n", hart_id);
                // Handle task completion logic here
                break;
                
            case IRQ_DMA512:
                printf("[Hart-%d-Handler] Processing DMA completion interrupt\n", hart_id);
                // Handle DMA completion logic here
                break;
                
            case IRQ_GPIO:
                printf("[Hart-%d-Handler] Processing error interrupt\n", hart_id);
                // Handle error processing logic here
                break;
                
            case IRQ_PIT:
                printf("[Hart-%d-Handler] Processing timer interrupt\n", hart_id);
                // Handle timer logic here
                break;
                
            case IRQ_SPI1:
                printf("[Hart-%d-Handler] Processing resource request interrupt\n", hart_id);
                // Handle resource request logic here
                break;
                
            case IRQ_RTC_ALARM:
                printf("[Hart-%d-Handler] Processing shutdown request interrupt\n", hart_id);
                // Handle shutdown logic here
                break;
                
            default:
                printf("[Hart-%d-Handler] Unknown interrupt source %d\n", hart_id, claimed_irq);
                break;
        }
        
        // Complete the interrupt (mandatory in PLIC flow)
        PLIC_M_TAR_comp_write((PLIC_RegDef*)plic, tgt_local, claimed_irq);
        printf("[Hart-%d-PLIC] Completed interrupt %s (%d)\n", 
               hart_id, get_plic_irq_name(claimed_irq), claimed_irq);
        
        interrupts_handled++;
    }
    
    return interrupts_handled;
} 