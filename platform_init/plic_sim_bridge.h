#ifndef PLIC_SIM_BRIDGE_H
#define PLIC_SIM_BRIDGE_H

/**
 * @brief Resets all PLIC simulation instances to a clean state.
 *
 * This function iterates through all known PLIC hardware base addresses,
 * maps them to their corresponding simulation memory pointers, and calls
 * the HAL's PLIC_clear function on them. This provides a definitive
 * reset of the PLIC hardware simulation, ensuring that tests run in a
 * clean environment free from any state left over from initial platform
 * setup.
 */
void plic_sim_bridge_reset_all(void);

#endif // PLIC_SIM_BRIDGE_H 