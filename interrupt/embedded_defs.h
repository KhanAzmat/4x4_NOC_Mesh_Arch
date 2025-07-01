#ifndef EMBEDDED_DEFS_H
#define EMBEDDED_DEFS_H

// Define __IO macro for memory-mapped registers
#define __IO volatile

// Add any other missing embedded definitions
#define __I  volatile const   // Read-only
#define __O  volatile         // Write-only

#endif
