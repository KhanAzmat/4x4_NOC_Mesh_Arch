#include "plic_sim_bridge.h"
#include "plic.h"
#include "address_manager.h" // For addr_to_ptr
#include <string.h>          // For memset

// Array of known hardware base addresses for all PLIC instances in the system.
static const uint64_t plic_hw_bases[] = {
    PLIC_0_C0C1_BASE,
    PLIC_0_NXY_BASE,
    PLIC_1_C0C1_BASE,
    PLIC_1_NXY_BASE,
    PLIC_2_C0C1_BASE,
    PLIC_2_NXY_BASE
};

void plic_sim_bridge_reset_all(void)
{
    size_t num_plics = sizeof(plic_hw_bases) / sizeof(plic_hw_bases[0]);

    for (size_t i = 0; i < num_plics; ++i) {
        uint64_t hw_addr = plic_hw_bases[i];
        void* sim_ptr = addr_to_ptr(hw_addr);

        if (sim_ptr) {
            volatile PLIC_RegDef* plic_inst = (volatile PLIC_RegDef*)sim_ptr;

            // CRITICAL FIX: Do not use the faulty PLIC_clear from the HAL.
            // Instead, perform a direct, manual reset of the simulated hardware registers.

            // 1. Clear all source priority registers.
            for (int j = 0; j < N_SPRIO_REGS; ++j) {
                plic_inst->sprio_regs[j] = 0;
            }

            // 2. Clear all pending bits.
            for (int j = 0; j < N_PEND_REGS; ++j) {
                plic_inst->pending_regs[j] = 0;
            }
            
            // 3. Disable all interrupts for all targets.
            for (int j = 0; j < N_TARGET_EN; ++j) {
                for (int k = 0; k < N_TAR_ENB_REG; ++k) {
                    plic_inst->teregs[j].regs[k] = 0;
                }
            }

            // 4. Reset all target thresholds and claim/complete registers to 0.
            for (int j = 0; j < N_TARGET_EN; ++j) {
                plic_inst->tpcregs[j].tar_prio_thres = 0;
                plic_inst->tpcregs[j].tar_claim_comp = 0;
            }

            // 5. Clear feature enable register.
            plic_inst->feature_enable_reg = 0;
        }
    }
} 