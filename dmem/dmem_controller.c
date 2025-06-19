#include "dmem_controller.h"
#include "platform_init/address_manager.h"
#include <string.h>
#include <stdio.h>

// Base symbol functions for DMEM register access
void dmem_write_reg(uint64_t reg_addr, uint32_t value) {
    // In hardware, this would write to DMEM control registers
    // For simulation, we validate the register address is valid
    if (get_address_region(reg_addr) == ADDR_DMEM_512) {
        uint32_t* reg_ptr = (uint32_t*)addr_to_ptr(reg_addr);
        if (reg_ptr) {
            *reg_ptr = value;
        }
    }
}

uint32_t dmem_read_reg(uint64_t reg_addr) {
    // In hardware, this would read from DMEM control registers
    if (get_address_region(reg_addr) == ADDR_DMEM_512) {
        uint32_t* reg_ptr = (uint32_t*)addr_to_ptr(reg_addr);
        if (reg_ptr) {
            return *reg_ptr;
        }
    }
    return 0;
}

// Driver functions that use base symbols
int dmem_read(int dmem_id, uint64_t offset, uint8_t* buffer, size_t size) {
    if (dmem_id < 0 || dmem_id >= NUM_DMEMS || !buffer) return -1;
    
    // Calculate DMEM base address using memory map
    uint64_t dmem_bases[] = {
        DMEM0_512_BASE, DMEM1_512_BASE, DMEM2_512_BASE, DMEM3_512_BASE,
        DMEM4_512_BASE, DMEM5_512_BASE, DMEM6_512_BASE, DMEM7_512_BASE
    };
    
    uint64_t src_addr = dmem_bases[dmem_id] + offset;
    
    // Validate address range
    if (!validate_address(src_addr, size)) return -1;
    
    // Use base symbol layer - addr_to_ptr for hardware access simulation
    uint8_t* src_ptr = addr_to_ptr(src_addr);
    if (!src_ptr) return -1;
    
    // Simulate DMEM read operation
    memcpy(buffer, src_ptr, size);
    return (int)size;
}

int dmem_write(int dmem_id, uint64_t offset, const uint8_t* buffer, size_t size) {
    if (dmem_id < 0 || dmem_id >= NUM_DMEMS || !buffer) return -1;
    
    // Calculate DMEM base address using memory map
    uint64_t dmem_bases[] = {
        DMEM0_512_BASE, DMEM1_512_BASE, DMEM2_512_BASE, DMEM3_512_BASE,
        DMEM4_512_BASE, DMEM5_512_BASE, DMEM6_512_BASE, DMEM7_512_BASE
    };
    
    uint64_t dst_addr = dmem_bases[dmem_id] + offset;
    
    // Validate address range
    if (!validate_address(dst_addr, size)) return -1;
    
    // Use base symbol layer - addr_to_ptr for hardware access simulation
    uint8_t* dst_ptr = addr_to_ptr(dst_addr);
    if (!dst_ptr) return -1;
    
    // Simulate DMEM write operation
    memcpy(dst_ptr, buffer, size);
    return (int)size;
}

int dmem_copy(uint64_t src_addr, uint64_t dst_addr, size_t size) {
    // Driver validates both addresses are DMEM regions
    if (get_address_region(src_addr) != ADDR_DMEM_512 || 
        get_address_region(dst_addr) != ADDR_DMEM_512) {
        return -1;
    }
    
    // Validate addresses
    if (!validate_address(src_addr, size) || !validate_address(dst_addr, size)) {
        return -1;
    }
    
    // Use base symbol layer for hardware access
    uint8_t* src_ptr = addr_to_ptr(src_addr);
    uint8_t* dst_ptr = addr_to_ptr(dst_addr);
    
    if (!src_ptr || !dst_ptr) return -1;
    
    // Simulate DMEM-to-DMEM copy operation
    memcpy(dst_ptr, src_ptr, size);
    return (int)size;
}

int dmem_get_status(int dmem_id) {
    if (dmem_id < 0 || dmem_id >= NUM_DMEMS) return -1;
    
    // In real hardware, would read status registers
    // For simulation, always return ready status
    return 0; // Ready
}

int dmem_init(int dmem_id) {
    if (dmem_id < 0 || dmem_id >= NUM_DMEMS) return -1;
    
    // In real hardware, would initialize DMEM controller
    // For simulation, just validate the DMEM exists
    uint64_t dmem_bases[] = {
        DMEM0_512_BASE, DMEM1_512_BASE, DMEM2_512_BASE, DMEM3_512_BASE,
        DMEM4_512_BASE, DMEM5_512_BASE, DMEM6_512_BASE, DMEM7_512_BASE
    };
    
    uint64_t dmem_addr = dmem_bases[dmem_id];
    return validate_address(dmem_addr, 1) ? 0 : -1;
} 