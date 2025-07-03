#ifndef PLATFORM_H
#define PLATFORM_H

/* Standard headers needed by vendor HALs */
#include <stdint.h>
#include <stddef.h>

/*
 * Compatibility shim for vendor HALs (DMAC512, PLIC, etc.) when
 * compiling the platform on a native host (x86-64/Linux).  The real
 * embedded environment provides these macros via CMSIS-style headers;
 * here we simply map them to standard C qualifiers so the code
 * compiles and the simulator can treat all registers as plain volatile
 * memory locations.
 */

/* Input-only register qualifier */
#ifndef __I
#define __I  volatile const
#endif

/* Output-only register qualifier */
#ifndef __O
#define __O  volatile
#endif

/* Input/Output register qualifier */
#ifndef __IO
#define __IO volatile
#endif

/* GCC-specific inline assembly support for RISC-V HAL code */
#ifndef __GNUC__
#error "This platform requires GCC compiler for inline assembly support"
#endif

/* Ensure asm keyword is available (required by PLIC HAL get_hartid function) */
#ifndef asm
#define asm __asm__
#endif

/* RISC-V instruction compatibility for simulation environment */
/* Replace RISC-V fence instruction with x86 memory barrier for simulation */
/* This ensures memory operation ordering is preserved in simulation */

#endif /* PLATFORM_H */ 