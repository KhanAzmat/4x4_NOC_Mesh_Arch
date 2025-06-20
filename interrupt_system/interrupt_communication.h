#ifndef INTERRUPT_COMMUNICATION_H
#define INTERRUPT_COMMUNICATION_H

#include <sys/socket.h>
#include <sys/un.h>
#include <pthread.h>
#include "interrupt_types.h"

#define INTERRUPT_SOCKET_PATH_BASE "/tmp/noc_interrupt"
#define INTERRUPT_SOCKET_BUFFER_SIZE 1024
#define MAX_COMMUNICATION_RETRIES 3
#define COMMUNICATION_TIMEOUT_MS 100

// Communication method types
typedef enum {
    COMM_METHOD_UNIX_SOCKET = 0,   // Unix domain sockets (default)
    COMM_METHOD_SHARED_MEMORY = 1,  // Shared memory with semaphores
    COMM_METHOD_PIPE = 2           // Named pipes
} communication_method_t;

// Unix socket communication structure
typedef struct {
    int server_fd;                 // Server socket file descriptor
    int client_fd;                 // Client socket file descriptor
    struct sockaddr_un server_addr; // Server address
    struct sockaddr_un client_addr; // Client address
    char socket_path[256];         // Socket file path
    pthread_mutex_t socket_lock;   // Protects socket operations
} unix_socket_comm_t;

// Shared memory communication structure (for future use)
typedef struct {
    void* shared_region;           // Shared memory region
    size_t region_size;            // Size of shared region
    int semaphore_id;              // Semaphore for synchronization
    char shm_name[64];             // Shared memory name
} shared_memory_comm_t;

// Communication interface - abstracts different communication methods
typedef struct {
    communication_method_t method; // Communication method in use
    bool is_initialized;           // Initialization status
    bool is_server;                // True if acting as server (C0), false if client (tile)
    int entity_id;                 // Entity ID (0 for C0, 1-7 for tiles)
    
    union {
        unix_socket_comm_t socket;     // Unix socket communication
        shared_memory_comm_t shared_mem; // Shared memory communication
    } comm;
    
    // Statistics
    uint64_t messages_sent;
    uint64_t messages_received;
    uint64_t send_failures;
    uint64_t receive_failures;
    uint64_t bytes_sent;
    uint64_t bytes_received;
    
} interrupt_communication_t;

// Communication interface functions
int interrupt_comm_init(interrupt_communication_t* icomm, communication_method_t method, bool is_server, int entity_id);
int interrupt_comm_destroy(interrupt_communication_t* icomm);

// Message transmission
int interrupt_comm_send_irq(interrupt_communication_t* icomm, int target_id, interrupt_request_t* irq);
int interrupt_comm_receive_irq(interrupt_communication_t* icomm, interrupt_request_t* irq);

// Connection management
int interrupt_comm_listen(interrupt_communication_t* icomm);
int interrupt_comm_connect(interrupt_communication_t* icomm, int target_id);
int interrupt_comm_accept(interrupt_communication_t* icomm);
int interrupt_comm_disconnect(interrupt_communication_t* icomm);

// Unix socket specific functions
int unix_socket_init_server(unix_socket_comm_t* sock, int server_id);
int unix_socket_init_client(unix_socket_comm_t* sock, int client_id);
int unix_socket_send_irq(unix_socket_comm_t* sock, interrupt_request_t* irq);
int unix_socket_receive_irq(unix_socket_comm_t* sock, interrupt_request_t* irq);
int unix_socket_cleanup(unix_socket_comm_t* sock);

// Utility functions
const char* get_socket_path(int entity_id);
int set_socket_timeout(int sockfd, int timeout_ms);
int serialize_irq(interrupt_request_t* irq, char* buffer, size_t buffer_size);
int deserialize_irq(const char* buffer, interrupt_request_t* irq);

// Statistics and monitoring
void interrupt_comm_print_statistics(interrupt_communication_t* icomm);
void interrupt_comm_reset_statistics(interrupt_communication_t* icomm);

// Error handling
const char* interrupt_comm_strerror(int error_code);

#endif // INTERRUPT_COMMUNICATION_H 