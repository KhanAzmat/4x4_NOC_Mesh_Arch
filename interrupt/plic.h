#ifndef _PLIC_H
#define _PLIC_H
 
#include <stdint.h>
#include "printf.h"
// #include "aradlm.h"
#include "generated/mem_map.h"  // Add your platform memory map

// Define __IO macro for memory-mapped I/O registers
#ifndef __IO
#define __IO volatile
#endif

#ifndef __I
#define __I volatile const
#endif

#ifndef __O  
#define __O volatile
#endif

// *** ADD THESE MISSING CONSTANTS: ***
#define PKT_INTERRUPT_REQ  0x5    // ← ADD THIS
// uint32_t current_hart_id = 0;     // ← ADD THIS

#define NR_HARTS        8  // Changed from 25 to 8 for your platform
#define SOURCE_BASE_ID  32 // Keep same - reserve first 31 pending bits
#define SLOT_PER_TARGET 8  // Changed from 25 to 8 (match your tile count)

#define PLIC_0_C0C1_BASE  0x90000000UL  // Changed to fit your memory map
#define PLIC_0_NXY_BASE   0x90400000UL  // Changed to fit your memory map

#define PLIC_1_C0C1_BASE  0x90800000UL  // Changed (though you may not need PLIC1)
#define PLIC_1_NXY_BASE   0x90C00000UL  // Changed (though you may not need PLIC1)

#define PLIC_2_C0C1_BASE  0x91000000UL  // Changed (though you may not need PLIC2)
#define PLIC_2_NXY_BASE   0x91400000UL  // Changed (though you may not need PLIC2)

#define PLIC_SIZE       0x0000000000400000UL  // Keep same - 4MB per PLIC

// Update PLIC target mapping for your 8-tile platform
static const uint8_t PLIC_TARGET_BASE[3]  = { 0,  8, 16 };   // Updated for 8 tiles
static const uint8_t PLIC_TARGET_COUNT[3] = { 8,  0,  0 };   // Only need first PLIC

#define N_SPRIO_REGS  1023
#define N_PEND_REGS   32
#define N_TRIG_REGS   32
 
#define N_TAR_ENB_REG 32
#define N_TARGET_EN   16
 
#define N_TAR_PREEMP_STACK 8
#define N_TARGET_PC        16
 
#define GET_C0C1_PLICX_BASE(x) \
    (PLIC_0_C0C1_BASE + ((x) * PLIC_SIZE))
 
#define GET_NXY_PLICX_BASE(x) \
    (PLIC_0_NXY_BASE + ((x) * PLIC_SIZE))
 
typedef struct __tar_enb_reg {
    __IO uint32_t regs[32];
} tar_enb_regs;
 
typedef struct __tar_prio_claim {
    __IO uint32_t tar_prio_thres;
    __IO uint32_t tar_claim_comp;
    __IO uint8_t reserved1[0x3f8];
    __IO uint32_t preempt_prio_stack[N_TAR_PREEMP_STACK];
    __IO uint8_t reserved2[0xbe0];
} tar_prio_claim;
 
typedef struct __PLIC {
    __IO uint32_t feature_enable_reg;        // 0x0 ~ 0x4
    __IO uint32_t sprio_regs[N_SPRIO_REGS];  // 0x4 ~ 0xFFFC
    __IO uint32_t pending_regs[N_PEND_REGS]; // 0x1000 ~ 0x107C
    __IO uint32_t trigger_regs[N_TRIG_REGS]; // 0x1080 ~ 0x10FC
    __IO uint32_t num_tar_intp;              // 0x1100 ~ 0x1104
    __IO uint32_t ver_max_prio;              // 0x1104 ~ 0x1108
 
    // This is pending.
    __IO uint8_t reserved1[0xef8];           // 0x1108 ~ 0x2000
 
    tar_enb_regs teregs[N_TARGET_EN];   // 0x2000 ~ 0x2780
 
    // This is pending
    __IO uint8_t reserved2[0x1fd800];        // 0x2800 ~ 0x20_0000
 
    tar_prio_claim tpcregs[N_TARGET_PC];// 0x20_0000 ~ 0x20_FFFF
    
    // This is pending
    __IO uint8_t reserved3[0x1f0000];        // 0x21_0000 ~ 0x40_0000
} PLIC_RegDef;
 
enum PLIC_FEATURE_TYPE {
    PREEMPT,
    VECTORED
};

typedef enum {
    IRQ_WDT,
    IRQ_RTC_PERIOD,
    IRQ_RTC_ALARM,
    IRQ_PIT,
    IRQ_SPI1,
    IRQ_SPI2,
    IRQ_I2C,
    IRQ_GPIO,
    IRQ_UART1,
    IRQ_USB_HOST,
    IRQ_DMA,
    IRQ_DMA512,
    IRQ_MESH_NODE = 20,
    IRQ_FX3 = 21,
    
    // Enhanced interrupt types for bidirectional mesh communication
    IRQ_TASK_COMPLETE = 22,
    IRQ_TASK_ASSIGN = 23,
    IRQ_ERROR_REPORT = 24,
    IRQ_DMA_COMPLETE = 25,
    IRQ_SYNC_REQUEST = 26,
    IRQ_SYNC_RESPONSE = 27,
    IRQ_SHUTDOWN_REQUEST = 28,
} irq_source_id_t;

// Interrupt direction types
typedef enum {
    INT_DIR_TO_C0,      // Processing nodes -> C0 (original pattern)
    INT_DIR_FROM_C0,    // C0 -> Processing nodes (new capability)
    INT_DIR_PEER_TO_PEER // Direct tile-to-tile (future extension)
} interrupt_direction_t;

extern uint32_t current_hart_id;  // Declare external variable

static inline uint32_t get_hartid(void) {
    // uint64_t hartid;
    // asm volatile("csrr %0, mhartid" : "=r"(hartid));
    // return (uint32_t)hartid;

    // Use platform-specific hart ID
    return current_hart_id;
}

int PLIC_version(PLIC_RegDef*);
int PLIC_max_prio(PLIC_RegDef*);
int PLIC_num_tar(PLIC_RegDef*);
int PLIC_num_intr(PLIC_RegDef*);
 
/**
* @brief Initialize a plic instance accord to hartid.

* @param  obj plic instance accord to hartid.
* @param Initialize a plic instance accord to hartid.
*/
void PLIC_init(volatile PLIC_RegDef **obj, uint8_t which);

/**
* @brief Clear the memory section of assigned plic instance.

* @param  obj plic instance accord to hartid.
* @param Initialize a plic instance accord to hartid.
*/
void PLIC_clear(PLIC_RegDef *obj);
 
/**
* @brief Set the feature enable register.

* @param  obj plic instance accord to hartid.
* @param Initialize a plic instance accord to hartid.
*/
void PLIC_feature_set(PLIC_RegDef *obj, enum PLIC_FEATURE_TYPE type);

/**
* @brief Clear the feature enable register.

* @param  obj plic instance accord to hartid.
* @param Initialize a plic instance accord to hartid.
*/
void PLIC_feature_clear(PLIC_RegDef *obj, enum PLIC_FEATURE_TYPE type);


/**
* @brief Set the priority level for a specific source.

* @param  obj plic instance accord to hartid.
* @param Initialize a plic instance accord to hartid.
*/
int PLIC_N_priority_set(PLIC_RegDef *obj, uint32_t source, uint8_t priority);

/**
* @brief Clear the priority level for a specific source.

* @param  obj plic instance accord to hartid.
* @param Initialize a plic instance accord to hartid.
*/
int PLIC_N_priority_clear(PLIC_RegDef *obj, uint32_t source, uint8_t priority);
 
/**
* @brief Read the interrupt pending register.

* @param  obj plic instance accord to hartid.
* @param Initialize a plic instance accord to hartid.
*/
int PLIC_N_source_pending_read(PLIC_RegDef *obj, uint32_t source);

/**
* @brief Set the interrupt pending register.

* @param  obj plic instance accord to hartid.
* @param Initialize a plic instance accord to hartid.
*/
int PLIC_N_source_pending_write(PLIC_RegDef *obj, uint32_t source);
 
/**
* @brief Read the trigger type register.

* @param  obj plic instance accord to hartid.
* @param Initialize a plic instance accord to hartid.
*/
int PLIC_N_source_tri_type_read(PLIC_RegDef *obj, uint32_t source);

/**
* @brief Write the trigger type register.

* @param  obj plic instance accord to hartid.
* @param Initialize a plic instance accord to hartid.
*/
int PLIC_N_source_tri_type_write(PLIC_RegDef *obj, uint32_t source);
 
/**
* @brief Enable interrupt source for a specific target.

* @param  obj plic instance accord to hartid.
* @param Initialize a plic instance accord to hartid.
*/
int PLIC_M_TAR_enable(PLIC_RegDef *obj, uint32_t target, uint32_t source);


int PLIC_M_TAR_read(PLIC_RegDef *obj, uint32_t target, uint32_t source);

/**
* @brief Disable interrupt source for a specific target.

* @param  obj plic instance accord to hartid.
* @param Initialize a plic instance accord to hartid.
*/
int PLIC_M_TAR_disable(PLIC_RegDef *obj, uint32_t target, uint32_t source);
 
/**
* @brief Read the claim register for a sepcific target.

* @param  obj plic instance accord to hartid.
* @param Initialize a plic instance accord to hartid.
*/
int PLIC_M_TAR_claim_read(PLIC_RegDef *obj, uint32_t target);

/**
* @brief Write the interrupt ID to claim and complete register.

* @param  obj plic instance accord to hartid.
* @param Initialize a plic instance accord to hartid.
*/
int PLIC_M_TAR_comp_write(PLIC_RegDef *obj, uint32_t target, uint32_t interrupt_id);
 
int PLIC_M_TAR_thre_write(PLIC_RegDef *obj, uint8_t tar, uint32_t thres);
int PLIC_M_TAR_thre_read(PLIC_RegDef *obj, uint8_t tar);
 

int PLIC_trigger_interrupt(uint32_t source_hart_id, uint32_t target_hartid);

void plic_init_for_this_hart(uint32_t hartid);

void plic_select(uint32_t hartid, volatile PLIC_RegDef **out_plic, uint32_t *out_tgt_local);

void PLIC_enable_interrupt(irq_source_id_t irq_id, uint32_t hart_id);

void PLIC_set_priority(irq_source_id_t irq_id, uint32_t hart_id, uint32_t prior);

void PLIC_set_threshold(uint32_t hart_id, uint32_t threshold);

// Enhanced PLIC functions for bidirectional communication
uint32_t PLIC_calculate_source_id(uint32_t source_hart, uint32_t target_hart, irq_source_id_t irq_type);
int PLIC_setup_bidirectional_interrupts(void);
int PLIC_trigger_typed_interrupt(uint32_t source_hart, uint32_t target_hart, irq_source_id_t irq_type);

#endif