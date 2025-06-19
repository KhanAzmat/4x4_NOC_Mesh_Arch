#ifndef DMEM_CONTROLLER_H
#define DMEM_CONTROLLER_H

#include <stdint.h>
#include <stddef.h>
#include "generated/mem_map.h"

// DMEM controller driver functions
int dmem_read(int dmem_id, uint64_t offset, uint8_t* buffer, size_t size);
int dmem_write(int dmem_id, uint64_t offset, const uint8_t* buffer, size_t size);
int dmem_copy(uint64_t src_addr, uint64_t dst_addr, size_t size);

// Base symbol functions for DMEM register access
void dmem_write_reg(uint64_t reg_addr, uint32_t value);
uint32_t dmem_read_reg(uint64_t reg_addr);

// DMEM status and control
int dmem_get_status(int dmem_id);
int dmem_init(int dmem_id);

#endif 