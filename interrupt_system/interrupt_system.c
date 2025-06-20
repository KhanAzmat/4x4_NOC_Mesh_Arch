#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include "interrupt_system.h"

// Global interrupt system instance
interrupt_system_t g_interrupt_system = {0};

// ============================================================================
// MAIN API FUNCTIONS
// ============================================================================

// Initialize interrupt system
int interrupt_system_init(int entity_id, communication_method_t comm_method) {
    if (g_interrupt_system.system_initialized) {
        printf("WARNING: Interrupt system already initialized\n");
        return IRQ_SYSTEM_ERROR_ALREADY;
    }
    
    if (entity_id < 0 || entity_id >= MAX_TILES) {
        printf("ERROR: Invalid entity ID %d (must be 0-7)\n", entity_id);
        return IRQ_SYSTEM_ERROR_INVALID;
    }
    
    memset(&g_interrupt_system, 0, sizeof(g_interrupt_system));
    
    g_interrupt_system.entity_id = entity_id;
    g_interrupt_system.is_c0_master = (entity_id == 0);
    g_interrupt_system.comm_method = comm_method;
    g_interrupt_system.enable_statistics = true;
    g_interrupt_system.enable_debug = false;
    
    int result = 0;
    
    if (g_interrupt_system.is_c0_master) {
        // Initialize C0 master components
        printf("INFO: Initializing interrupt system for C0 master\n");
        
        // Allocate and initialize interrupt controller
        g_interrupt_system.c0_controller = malloc(sizeof(interrupt_controller_t));
        if (!g_interrupt_system.c0_controller) {
            printf("ERROR: Failed to allocate memory for C0 controller\n");
            return IRQ_SYSTEM_ERROR_SYSTEM;
        }
        
        result = interrupt_controller_init(g_interrupt_system.c0_controller);
        if (result != 0) {
            printf("ERROR: Failed to initialize C0 interrupt controller\n");
            free(g_interrupt_system.c0_controller);
            g_interrupt_system.c0_controller = NULL;
            return IRQ_SYSTEM_ERROR_SYSTEM;
        }
        
        // Register default ISR handlers
        interrupt_register_isr(g_interrupt_system.c0_controller, IRQ_TYPE_TASK_COMPLETE, default_task_complete_isr);
        interrupt_register_isr(g_interrupt_system.c0_controller, IRQ_TYPE_ERROR, default_error_isr);
        interrupt_register_isr(g_interrupt_system.c0_controller, IRQ_TYPE_DMA_COMPLETE, default_dma_complete_isr);
        interrupt_register_isr(g_interrupt_system.c0_controller, IRQ_TYPE_RESOURCE_REQUEST, default_resource_request_isr);
        interrupt_register_isr(g_interrupt_system.c0_controller, IRQ_TYPE_SHUTDOWN, default_shutdown_isr);
        
        // Allocate and initialize communication interface
        g_interrupt_system.c0_comm = malloc(sizeof(interrupt_communication_t));
        if (!g_interrupt_system.c0_comm) {
            printf("ERROR: Failed to allocate memory for C0 communication\n");
            interrupt_controller_destroy(g_interrupt_system.c0_controller);
            free(g_interrupt_system.c0_controller);
            g_interrupt_system.c0_controller = NULL;
            return IRQ_SYSTEM_ERROR_SYSTEM;
        }
        
        result = interrupt_comm_init(g_interrupt_system.c0_comm, comm_method, true, entity_id);
        if (result != 0) {
            printf("ERROR: Failed to initialize C0 communication interface\n");
            interrupt_controller_destroy(g_interrupt_system.c0_controller);
            free(g_interrupt_system.c0_controller);
            free(g_interrupt_system.c0_comm);
            g_interrupt_system.c0_controller = NULL;
            g_interrupt_system.c0_comm = NULL;
            return IRQ_SYSTEM_ERROR_COMM;
        }
        
    } else {
        // Initialize tile components
        printf("INFO: Initializing interrupt system for tile %d\n", entity_id);
        
        // Allocate and initialize tile interface
        g_interrupt_system.tile_interface = malloc(sizeof(tile_interrupt_t));
        if (!g_interrupt_system.tile_interface) {
            printf("ERROR: Failed to allocate memory for tile interface\n");
            return IRQ_SYSTEM_ERROR_SYSTEM;
        }
        
        result = tile_interrupt_init(g_interrupt_system.tile_interface, entity_id);
        if (result != 0) {
            printf("ERROR: Failed to initialize tile interrupt interface\n");
            free(g_interrupt_system.tile_interface);
            g_interrupt_system.tile_interface = NULL;
            return IRQ_SYSTEM_ERROR_SYSTEM;
        }
        
        // Register default ISR handlers for incoming IRQs
        tile_register_incoming_isr(g_interrupt_system.tile_interface, IRQ_TYPE_SHUTDOWN, default_tile_shutdown_isr);
        tile_register_incoming_isr(g_interrupt_system.tile_interface, IRQ_TYPE_RESOURCE_REQUEST, default_tile_resource_grant_isr);
        tile_register_incoming_isr(g_interrupt_system.tile_interface, IRQ_TYPE_CUSTOM, default_tile_config_update_isr);
        
        // Allocate and initialize communication interface
        g_interrupt_system.tile_comm = malloc(sizeof(interrupt_communication_t));
        if (!g_interrupt_system.tile_comm) {
            printf("ERROR: Failed to allocate memory for tile communication\n");
            tile_interrupt_destroy(g_interrupt_system.tile_interface);
            free(g_interrupt_system.tile_interface);
            g_interrupt_system.tile_interface = NULL;
            return IRQ_SYSTEM_ERROR_SYSTEM;
        }
        
        result = interrupt_comm_init(g_interrupt_system.tile_comm, comm_method, false, entity_id);
        if (result != 0) {
            printf("ERROR: Failed to initialize tile communication interface\n");
            tile_interrupt_destroy(g_interrupt_system.tile_interface);
            free(g_interrupt_system.tile_interface);
            free(g_interrupt_system.tile_comm);
            g_interrupt_system.tile_interface = NULL;
            g_interrupt_system.tile_comm = NULL;
            return IRQ_SYSTEM_ERROR_COMM;
        }
    }
    
    g_interrupt_system.system_initialized = true;
    
    printf("INFO: Interrupt system initialized successfully for entity %d (%s)\n", 
           entity_id, g_interrupt_system.is_c0_master ? "C0 Master" : "Tile");
    
    return IRQ_SYSTEM_SUCCESS;
}

// Destroy interrupt system
int interrupt_system_destroy(void) {
    if (!g_interrupt_system.system_initialized) {
        return IRQ_SYSTEM_ERROR_NOT_INIT;
    }
    
    printf("INFO: Destroying interrupt system for entity %d\n", g_interrupt_system.entity_id);
    
    if (g_interrupt_system.is_c0_master) {
        // Stop processing first
        interrupt_system_stop_c0_processing();
        
        // Destroy C0 components
        if (g_interrupt_system.c0_comm) {
            interrupt_comm_destroy(g_interrupt_system.c0_comm);
            free(g_interrupt_system.c0_comm);
            g_interrupt_system.c0_comm = NULL;
        }
        
        if (g_interrupt_system.c0_controller) {
            interrupt_controller_destroy(g_interrupt_system.c0_controller);
            free(g_interrupt_system.c0_controller);
            g_interrupt_system.c0_controller = NULL;
        }
    } else {
        // Stop processing first
        interrupt_system_stop_tile_processing();
        
        // Destroy tile components
        if (g_interrupt_system.tile_comm) {
            interrupt_comm_destroy(g_interrupt_system.tile_comm);
            free(g_interrupt_system.tile_comm);
            g_interrupt_system.tile_comm = NULL;
        }
        
        if (g_interrupt_system.tile_interface) {
            tile_interrupt_destroy(g_interrupt_system.tile_interface);
            free(g_interrupt_system.tile_interface);
            g_interrupt_system.tile_interface = NULL;
        }
    }
    
    g_interrupt_system.system_initialized = false;
    
    printf("INFO: Interrupt system destroyed\n");
    return IRQ_SYSTEM_SUCCESS;
}

// Check if system is initialized
bool interrupt_system_is_ready(void) {
    return g_interrupt_system.system_initialized;
}

// Get entity ID
int interrupt_system_get_entity_id(void) {
    if (!g_interrupt_system.system_initialized) {
        return -1;
    }
    return g_interrupt_system.entity_id;
}

// Check if this is C0 master
bool interrupt_system_is_c0_master(void) {
    return g_interrupt_system.system_initialized && g_interrupt_system.is_c0_master;
}

// ============================================================================
// C0 MASTER API
// ============================================================================

// Start C0 interrupt processing
int interrupt_system_start_c0_processing(void) {
    if (!g_interrupt_system.system_initialized || !g_interrupt_system.is_c0_master) {
        return IRQ_SYSTEM_ERROR_PERMISSION;
    }
    
    if (!g_interrupt_system.c0_controller) {
        return IRQ_SYSTEM_ERROR_NOT_INIT;
    }
    
    if (g_interrupt_system.c0_controller->processor_running) {
        printf("WARNING: C0 interrupt processing already running\n");
        return IRQ_SYSTEM_ERROR_ALREADY;
    }
    
    // Start processing thread
    g_interrupt_system.c0_controller->processor_running = true;
    
    int result = pthread_create(&g_interrupt_system.c0_controller->irq_processor_thread, 
                               NULL, interrupt_processor_thread_main, 
                               g_interrupt_system.c0_controller);
    
    if (result != 0) {
        printf("ERROR: Failed to create C0 IRQ processor thread: %s\n", strerror(result));
        g_interrupt_system.c0_controller->processor_running = false;
        return IRQ_SYSTEM_ERROR_SYSTEM;
    }
    
    printf("INFO: C0 interrupt processing started\n");
    return IRQ_SYSTEM_SUCCESS;
}

// Stop C0 interrupt processing
int interrupt_system_stop_c0_processing(void) {
    if (!g_interrupt_system.system_initialized || !g_interrupt_system.is_c0_master) {
        return IRQ_SYSTEM_ERROR_PERMISSION;
    }
    
    if (!g_interrupt_system.c0_controller) {
        return IRQ_SYSTEM_ERROR_NOT_INIT;
    }
    
    if (!g_interrupt_system.c0_controller->processor_running) {
        return IRQ_SYSTEM_SUCCESS; // Already stopped
    }
    
    // Stop processing thread
    g_interrupt_system.c0_controller->processor_running = false;
    pthread_cond_signal(&g_interrupt_system.c0_controller->irq_available);
    
    // Wait for thread to finish
    pthread_join(g_interrupt_system.c0_controller->irq_processor_thread, NULL);
    
    printf("INFO: C0 interrupt processing stopped\n");
    return IRQ_SYSTEM_SUCCESS;
}

// Register C0 ISR
int interrupt_system_register_c0_isr(interrupt_type_t type, interrupt_isr_t handler) {
    if (!g_interrupt_system.system_initialized || !g_interrupt_system.is_c0_master) {
        return IRQ_SYSTEM_ERROR_PERMISSION;
    }
    
    if (!g_interrupt_system.c0_controller) {
        return IRQ_SYSTEM_ERROR_NOT_INIT;
    }
    
    interrupt_register_isr(g_interrupt_system.c0_controller, type, handler);
    return IRQ_SYSTEM_SUCCESS;
}

// Unregister C0 ISR
int interrupt_system_unregister_c0_isr(interrupt_type_t type) {
    if (!g_interrupt_system.system_initialized || !g_interrupt_system.is_c0_master) {
        return IRQ_SYSTEM_ERROR_PERMISSION;
    }
    
    if (!g_interrupt_system.c0_controller) {
        return IRQ_SYSTEM_ERROR_NOT_INIT;
    }
    
    interrupt_unregister_isr(g_interrupt_system.c0_controller, type);
    return IRQ_SYSTEM_SUCCESS;
}

// C0 control functions
int interrupt_system_c0_enable_tile(int tile_id) {
    if (!g_interrupt_system.system_initialized || !g_interrupt_system.is_c0_master) {
        return IRQ_SYSTEM_ERROR_PERMISSION;
    }
    
    if (!g_interrupt_system.c0_controller) {
        return IRQ_SYSTEM_ERROR_NOT_INIT;
    }
    
    return interrupt_enable_tile(g_interrupt_system.c0_controller, tile_id);
}

int interrupt_system_c0_disable_tile(int tile_id) {
    if (!g_interrupt_system.system_initialized || !g_interrupt_system.is_c0_master) {
        return IRQ_SYSTEM_ERROR_PERMISSION;
    }
    
    if (!g_interrupt_system.c0_controller) {
        return IRQ_SYSTEM_ERROR_NOT_INIT;
    }
    
    return interrupt_disable_tile(g_interrupt_system.c0_controller, tile_id);
}

int interrupt_system_c0_enable_type(interrupt_type_t type) {
    if (!g_interrupt_system.system_initialized || !g_interrupt_system.is_c0_master) {
        return IRQ_SYSTEM_ERROR_PERMISSION;
    }
    
    if (!g_interrupt_system.c0_controller) {
        return IRQ_SYSTEM_ERROR_NOT_INIT;
    }
    
    return interrupt_enable_type(g_interrupt_system.c0_controller, type);
}

int interrupt_system_c0_disable_type(interrupt_type_t type) {
    if (!g_interrupt_system.system_initialized || !g_interrupt_system.is_c0_master) {
        return IRQ_SYSTEM_ERROR_PERMISSION;
    }
    
    if (!g_interrupt_system.c0_controller) {
        return IRQ_SYSTEM_ERROR_NOT_INIT;
    }
    
    return interrupt_disable_type(g_interrupt_system.c0_controller, type);
}

// C0 monitoring functions
int interrupt_system_c0_get_queue_count(void) {
    if (!g_interrupt_system.system_initialized || !g_interrupt_system.is_c0_master) {
        return IRQ_SYSTEM_ERROR_PERMISSION;
    }
    
    if (!g_interrupt_system.c0_controller) {
        return IRQ_SYSTEM_ERROR_NOT_INIT;
    }
    
    return interrupt_queue_count(g_interrupt_system.c0_controller);
}

int interrupt_system_c0_get_queue_space(void) {
    if (!g_interrupt_system.system_initialized || !g_interrupt_system.is_c0_master) {
        return IRQ_SYSTEM_ERROR_PERMISSION;
    }
    
    if (!g_interrupt_system.c0_controller) {
        return IRQ_SYSTEM_ERROR_NOT_INIT;
    }
    
    return interrupt_queue_space_available(g_interrupt_system.c0_controller);
}

void interrupt_system_c0_print_statistics(void) {
    if (!g_interrupt_system.system_initialized || !g_interrupt_system.is_c0_master) {
        printf("ERROR: C0 statistics not available (not C0 master)\n");
        return;
    }
    
    if (!g_interrupt_system.c0_controller) {
        printf("ERROR: C0 controller not initialized\n");
        return;
    }
    
    interrupt_print_statistics(g_interrupt_system.c0_controller);
}

void interrupt_system_c0_reset_statistics(void) {
    if (!g_interrupt_system.system_initialized || !g_interrupt_system.is_c0_master) {
        return;
    }
    
    if (!g_interrupt_system.c0_controller) {
        return;
    }
    
    interrupt_reset_statistics(g_interrupt_system.c0_controller);
}

// Send IRQ from C0 to tile
int interrupt_system_c0_send_to_tile(int target_tile, interrupt_type_t type, 
                                     uint32_t data, const char* message) {
    if (!g_interrupt_system.system_initialized || !g_interrupt_system.is_c0_master) {
        return IRQ_SYSTEM_ERROR_PERMISSION;
    }
    
    if (!g_interrupt_system.c0_controller) {
        return IRQ_SYSTEM_ERROR_NOT_INIT;
    }
    
    return interrupt_send_to_tile(g_interrupt_system.c0_controller, target_tile, type, data, message);
}

// ============================================================================
// TILE API
// ============================================================================

// Start tile interrupt processing
int interrupt_system_start_tile_processing(void) {
    if (!g_interrupt_system.system_initialized || g_interrupt_system.is_c0_master) {
        return IRQ_SYSTEM_ERROR_PERMISSION;
    }
    
    if (!g_interrupt_system.tile_interface) {
        return IRQ_SYSTEM_ERROR_NOT_INIT;
    }
    
    if (g_interrupt_system.tile_interface->incoming_processor_running) {
        printf("WARNING: Tile interrupt processing already running\n");
        return IRQ_SYSTEM_ERROR_ALREADY;
    }
    
    // Start processing thread
    g_interrupt_system.tile_interface->incoming_processor_running = true;
    
    int result = pthread_create(&g_interrupt_system.tile_interface->incoming_processor_thread, 
                               NULL, tile_incoming_processor_thread_main, 
                               g_interrupt_system.tile_interface);
    
    if (result != 0) {
        printf("ERROR: Failed to create tile IRQ processor thread: %s\n", strerror(result));
        g_interrupt_system.tile_interface->incoming_processor_running = false;
        return IRQ_SYSTEM_ERROR_SYSTEM;
    }
    
    printf("INFO: Tile %d interrupt processing started\n", g_interrupt_system.entity_id);
    return IRQ_SYSTEM_SUCCESS;
}

// Stop tile interrupt processing
int interrupt_system_stop_tile_processing(void) {
    if (!g_interrupt_system.system_initialized || g_interrupt_system.is_c0_master) {
        return IRQ_SYSTEM_ERROR_PERMISSION;
    }
    
    if (!g_interrupt_system.tile_interface) {
        return IRQ_SYSTEM_ERROR_NOT_INIT;
    }
    
    if (!g_interrupt_system.tile_interface->incoming_processor_running) {
        return IRQ_SYSTEM_SUCCESS; // Already stopped
    }
    
    // Stop processing thread
    g_interrupt_system.tile_interface->incoming_processor_running = false;
    pthread_cond_signal(&g_interrupt_system.tile_interface->incoming_available);
    
    // Wait for thread to finish
    pthread_join(g_interrupt_system.tile_interface->incoming_processor_thread, NULL);
    
    printf("INFO: Tile %d interrupt processing stopped\n", g_interrupt_system.entity_id);
    return IRQ_SYSTEM_SUCCESS;
}

// Register tile ISR
int interrupt_system_register_tile_isr(interrupt_type_t type, tile_interrupt_isr_t handler) {
    if (!g_interrupt_system.system_initialized || g_interrupt_system.is_c0_master) {
        return IRQ_SYSTEM_ERROR_PERMISSION;
    }
    
    if (!g_interrupt_system.tile_interface) {
        return IRQ_SYSTEM_ERROR_NOT_INIT;
    }
    
    tile_register_incoming_isr(g_interrupt_system.tile_interface, type, handler);
    return IRQ_SYSTEM_SUCCESS;
}

// Unregister tile ISR
int interrupt_system_unregister_tile_isr(interrupt_type_t type) {
    if (!g_interrupt_system.system_initialized || g_interrupt_system.is_c0_master) {
        return IRQ_SYSTEM_ERROR_PERMISSION;
    }
    
    if (!g_interrupt_system.tile_interface) {
        return IRQ_SYSTEM_ERROR_NOT_INIT;
    }
    
    tile_unregister_incoming_isr(g_interrupt_system.tile_interface, type);
    return IRQ_SYSTEM_SUCCESS;
}

// Tile control functions
int interrupt_system_tile_enable_incoming_type(interrupt_type_t type) {
    if (!g_interrupt_system.system_initialized || g_interrupt_system.is_c0_master) {
        return IRQ_SYSTEM_ERROR_PERMISSION;
    }
    
    if (!g_interrupt_system.tile_interface) {
        return IRQ_SYSTEM_ERROR_NOT_INIT;
    }
    
    return tile_enable_incoming_type(g_interrupt_system.tile_interface, type);
}

int interrupt_system_tile_disable_incoming_type(interrupt_type_t type) {
    if (!g_interrupt_system.system_initialized || g_interrupt_system.is_c0_master) {
        return IRQ_SYSTEM_ERROR_PERMISSION;
    }
    
    if (!g_interrupt_system.tile_interface) {
        return IRQ_SYSTEM_ERROR_NOT_INIT;
    }
    
    return tile_disable_incoming_type(g_interrupt_system.tile_interface, type);
}

// Tile monitoring functions
int interrupt_system_tile_get_incoming_queue_count(void) {
    if (!g_interrupt_system.system_initialized || g_interrupt_system.is_c0_master) {
        return IRQ_SYSTEM_ERROR_PERMISSION;
    }
    
    if (!g_interrupt_system.tile_interface) {
        return IRQ_SYSTEM_ERROR_NOT_INIT;
    }
    
    return tile_incoming_queue_count(g_interrupt_system.tile_interface);
}

int interrupt_system_tile_get_incoming_queue_space(void) {
    if (!g_interrupt_system.system_initialized || g_interrupt_system.is_c0_master) {
        return IRQ_SYSTEM_ERROR_PERMISSION;
    }
    
    if (!g_interrupt_system.tile_interface) {
        return IRQ_SYSTEM_ERROR_NOT_INIT;
    }
    
    return tile_incoming_queue_space_available(g_interrupt_system.tile_interface);
}

void interrupt_system_tile_print_statistics(void) {
    if (!g_interrupt_system.system_initialized || g_interrupt_system.is_c0_master) {
        printf("ERROR: Tile statistics not available (not a tile)\n");
        return;
    }
    
    if (!g_interrupt_system.tile_interface) {
        printf("ERROR: Tile interface not initialized\n");
        return;
    }
    
    tile_print_interrupt_statistics(g_interrupt_system.tile_interface);
}

void interrupt_system_tile_reset_statistics(void) {
    if (!g_interrupt_system.system_initialized || g_interrupt_system.is_c0_master) {
        return;
    }
    
    if (!g_interrupt_system.tile_interface) {
        return;
    }
    
    tile_reset_interrupt_statistics(g_interrupt_system.tile_interface);
}

// Send IRQs from tile to C0
int interrupt_system_tile_send_to_c0(interrupt_type_t type, uint32_t data, const char* message) {
    if (!g_interrupt_system.system_initialized || g_interrupt_system.is_c0_master) {
        return IRQ_SYSTEM_ERROR_PERMISSION;
    }
    
    if (!g_interrupt_system.tile_interface) {
        return IRQ_SYSTEM_ERROR_NOT_INIT;
    }
    
    return tile_send_interrupt_to_c0(g_interrupt_system.tile_interface, type, data, message);
}

// Convenience functions
int interrupt_system_tile_signal_task_complete(uint32_t task_id) {
    return interrupt_system_tile_send_to_c0(IRQ_TYPE_TASK_COMPLETE, task_id, "Task completed");
}

int interrupt_system_tile_signal_error(uint32_t error_code, const char* error_msg) {
    return interrupt_system_tile_send_to_c0(IRQ_TYPE_ERROR, error_code, error_msg);
}

int interrupt_system_tile_signal_dma_complete(uint32_t transfer_id) {
    return interrupt_system_tile_send_to_c0(IRQ_TYPE_DMA_COMPLETE, transfer_id, "DMA transfer completed");
}

int interrupt_system_tile_request_resource(uint32_t resource_type) {
    return interrupt_system_tile_send_to_c0(IRQ_TYPE_RESOURCE_REQUEST, resource_type, "Resource request");
}

int interrupt_system_tile_signal_shutdown(void) {
    return interrupt_system_tile_send_to_c0(IRQ_TYPE_SHUTDOWN, 0, "Tile shutdown request");
}

// ============================================================================
// COMMON API
// ============================================================================

// Enable/disable debug
int interrupt_system_enable_debug(bool enable) {
    if (!g_interrupt_system.system_initialized) {
        return IRQ_SYSTEM_ERROR_NOT_INIT;
    }
    
    g_interrupt_system.enable_debug = enable;
    printf("INFO: Debug %s\n", enable ? "enabled" : "disabled");
    return IRQ_SYSTEM_SUCCESS;
}

// Enable/disable statistics
int interrupt_system_enable_statistics(bool enable) {
    if (!g_interrupt_system.system_initialized) {
        return IRQ_SYSTEM_ERROR_NOT_INIT;
    }
    
    g_interrupt_system.enable_statistics = enable;
    printf("INFO: Statistics %s\n", enable ? "enabled" : "disabled");
    return IRQ_SYSTEM_SUCCESS;
}

// Print communication statistics
void interrupt_system_print_comm_statistics(void) {
    if (!g_interrupt_system.system_initialized) {
        printf("ERROR: Interrupt system not initialized\n");
        return;
    }
    
    if (g_interrupt_system.is_c0_master && g_interrupt_system.c0_comm) {
        interrupt_comm_print_statistics(g_interrupt_system.c0_comm);
    } else if (!g_interrupt_system.is_c0_master && g_interrupt_system.tile_comm) {
        interrupt_comm_print_statistics(g_interrupt_system.tile_comm);
    } else {
        printf("ERROR: Communication interface not available\n");
    }
}

// Reset communication statistics
void interrupt_system_reset_comm_statistics(void) {
    if (!g_interrupt_system.system_initialized) {
        return;
    }
    
    if (g_interrupt_system.is_c0_master && g_interrupt_system.c0_comm) {
        interrupt_comm_reset_statistics(g_interrupt_system.c0_comm);
    } else if (!g_interrupt_system.is_c0_master && g_interrupt_system.tile_comm) {
        interrupt_comm_reset_statistics(g_interrupt_system.tile_comm);
    }
}

// Print system status
void interrupt_system_print_status(void) {
    printf("\n=== Interrupt System Status ===\n");
    printf("Initialized: %s\n", g_interrupt_system.system_initialized ? "Yes" : "No");
    
    if (g_interrupt_system.system_initialized) {
        printf("Entity ID: %d\n", g_interrupt_system.entity_id);
        printf("Role: %s\n", g_interrupt_system.is_c0_master ? "C0 Master" : "Tile");
        printf("Communication method: %s\n", 
               g_interrupt_system.comm_method == COMM_METHOD_UNIX_SOCKET ? "Unix Socket" : "Other");
        printf("Statistics enabled: %s\n", g_interrupt_system.enable_statistics ? "Yes" : "No");
        printf("Debug enabled: %s\n", g_interrupt_system.enable_debug ? "Yes" : "No");
        
        if (g_interrupt_system.is_c0_master) {
            printf("C0 Controller: %s\n", g_interrupt_system.c0_controller ? "Initialized" : "Not initialized");
            printf("C0 Communication: %s\n", g_interrupt_system.c0_comm ? "Initialized" : "Not initialized");
            if (g_interrupt_system.c0_controller) {
                printf("C0 Processing: %s\n", g_interrupt_system.c0_controller->processor_running ? "Running" : "Stopped");
            }
        } else {
            printf("Tile Interface: %s\n", g_interrupt_system.tile_interface ? "Initialized" : "Not initialized");
            printf("Tile Communication: %s\n", g_interrupt_system.tile_comm ? "Initialized" : "Not initialized");
            if (g_interrupt_system.tile_interface) {
                printf("Tile Processing: %s\n", g_interrupt_system.tile_interface->incoming_processor_running ? "Running" : "Stopped");
            }
        }
    }
    printf("===============================\n\n");
}

// Self test
int interrupt_system_self_test(void) {
    printf("INFO: Running interrupt system self-test...\n");
    
    if (!g_interrupt_system.system_initialized) {
        printf("ERROR: System not initialized\n");
        return IRQ_SYSTEM_ERROR_NOT_INIT;
    }
    
    // Test basic functionality based on role
    if (g_interrupt_system.is_c0_master) {
        printf("INFO: Testing C0 master functionality...\n");
        
        // Test queue operations
        int queue_count = interrupt_system_c0_get_queue_count();
        int queue_space = interrupt_system_c0_get_queue_space();
        printf("INFO: C0 queue status: %d/%d IRQs\n", queue_count, queue_space + queue_count);
        
    } else {
        printf("INFO: Testing tile functionality...\n");
        
        // Test queue operations
        int queue_count = interrupt_system_tile_get_incoming_queue_count();
        int queue_space = interrupt_system_tile_get_incoming_queue_space();
        printf("INFO: Tile incoming queue status: %d/%d IRQs\n", queue_count, queue_space + queue_count);
        
        // Test sending a test IRQ (just logs, doesn't actually send)
        interrupt_system_tile_send_to_c0(IRQ_TYPE_CUSTOM, 0xDEADBEEF, "Self-test IRQ");
    }
    
    printf("INFO: Self-test completed successfully\n");
    return IRQ_SYSTEM_SUCCESS;
}

// Get error string
const char* interrupt_system_strerror(int error_code) {
    switch (error_code) {
        case IRQ_SYSTEM_SUCCESS: return "Success";
        case IRQ_SYSTEM_ERROR_INVALID: return "Invalid argument";
        case IRQ_SYSTEM_ERROR_NOT_INIT: return "System not initialized";
        case IRQ_SYSTEM_ERROR_ALREADY: return "Already initialized/started";
        case IRQ_SYSTEM_ERROR_PERMISSION: return "Permission denied";
        case IRQ_SYSTEM_ERROR_COMM: return "Communication error";
        case IRQ_SYSTEM_ERROR_QUEUE_FULL: return "Queue full";
        case IRQ_SYSTEM_ERROR_TIMEOUT: return "Timeout";
        case IRQ_SYSTEM_ERROR_SYSTEM: return "System error";
        default: return "Unknown error";
    }
} 