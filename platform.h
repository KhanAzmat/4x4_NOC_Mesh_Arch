#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdint.h>
#include <stdbool.h>

// Define __IO macro for memory-mapped I/O registers (for software simulation)
#ifndef __IO
#define __IO volatile
#endif

// Memory synchronization for cross-platform compatibility
#ifndef __sync_synchronize
#define __sync_synchronize() __sync_synchronize()
#endif

#endif // PLATFORM_H 