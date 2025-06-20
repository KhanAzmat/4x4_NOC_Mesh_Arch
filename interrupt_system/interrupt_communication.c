#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include <fcntl.h>
#include "interrupt_communication.h"

// Initialize communication interface
int interrupt_comm_init(interrupt_communication_t* icomm, communication_method_t method, bool is_server, int entity_id) {
    if (!icomm) {
        printf("ERROR: interrupt_comm_init called with NULL pointer\n");
        return -1;
    }
    
    if (entity_id < 0 || entity_id >= MAX_TILES) {
        printf("ERROR: Invalid entity ID %d\n", entity_id);
        return -2;
    }
    
    memset(icomm, 0, sizeof(interrupt_communication_t));
    
    icomm->method = method;
    icomm->is_server = is_server;
    icomm->entity_id = entity_id;
    icomm->is_initialized = false;
    
    // Initialize based on communication method
    switch (method) {
        case COMM_METHOD_UNIX_SOCKET:
            if (is_server) {
                return unix_socket_init_server(&icomm->comm.socket, entity_id);
            } else {
                return unix_socket_init_client(&icomm->comm.socket, entity_id);
            }
            break;
            
        case COMM_METHOD_SHARED_MEMORY:
        case COMM_METHOD_PIPE:
            printf("ERROR: Communication method %d not yet implemented\n", method);
            return -3;
            
        default:
            printf("ERROR: Unknown communication method %d\n", method);
            return -4;
    }
}

// Destroy communication interface
int interrupt_comm_destroy(interrupt_communication_t* icomm) {
    if (!icomm || !icomm->is_initialized) {
        return -1;
    }
    
    switch (icomm->method) {
        case COMM_METHOD_UNIX_SOCKET:
            unix_socket_cleanup(&icomm->comm.socket);
            break;
            
        case COMM_METHOD_SHARED_MEMORY:
        case COMM_METHOD_PIPE:
            // TODO: Implement cleanup for other methods
            break;
    }
    
    icomm->is_initialized = false;
    printf("INFO: Communication interface destroyed for entity %d\n", icomm->entity_id);
    return 0;
}

// Send IRQ message
int interrupt_comm_send_irq(interrupt_communication_t* icomm, int target_id, interrupt_request_t* irq) {
    if (!icomm || !irq || !icomm->is_initialized) {
        return -1;
    }
    
    switch (icomm->method) {
        case COMM_METHOD_UNIX_SOCKET:
            return unix_socket_send_irq(&icomm->comm.socket, irq);
            
        case COMM_METHOD_SHARED_MEMORY:
        case COMM_METHOD_PIPE:
            printf("ERROR: Send not implemented for method %d\n", icomm->method);
            return -2;
            
        default:
            return -3;
    }
}

// Receive IRQ message
int interrupt_comm_receive_irq(interrupt_communication_t* icomm, interrupt_request_t* irq) {
    if (!icomm || !irq || !icomm->is_initialized) {
        return -1;
    }
    
    switch (icomm->method) {
        case COMM_METHOD_UNIX_SOCKET:
            return unix_socket_receive_irq(&icomm->comm.socket, irq);
            
        case COMM_METHOD_SHARED_MEMORY:
        case COMM_METHOD_PIPE:
            printf("ERROR: Receive not implemented for method %d\n", icomm->method);
            return -2;
            
        default:
            return -3;
    }
}

// Unix socket implementation

// Initialize Unix socket server (C0 master)
int unix_socket_init_server(unix_socket_comm_t* sock, int server_id) {
    if (!sock) {
        return -1;
    }
    
    memset(sock, 0, sizeof(unix_socket_comm_t));
    
    // Initialize mutex
    if (pthread_mutex_init(&sock->socket_lock, NULL) != 0) {
        printf("ERROR: Failed to initialize socket mutex\n");
        return -2;
    }
    
    // Create socket
    sock->server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock->server_fd == -1) {
        printf("ERROR: Failed to create server socket: %s\n", strerror(errno));
        pthread_mutex_destroy(&sock->socket_lock);
        return -3;
    }
    
    // Set up server address
    memset(&sock->server_addr, 0, sizeof(sock->server_addr));
    sock->server_addr.sun_family = AF_UNIX;
    snprintf(sock->socket_path, sizeof(sock->socket_path), "%s_%d.sock", INTERRUPT_SOCKET_PATH_BASE, server_id);
    strcpy(sock->server_addr.sun_path, sock->socket_path);
    
    // Remove existing socket file
    unlink(sock->socket_path);
    
    // Bind socket
    if (bind(sock->server_fd, (struct sockaddr*)&sock->server_addr, sizeof(sock->server_addr)) == -1) {
        printf("ERROR: Failed to bind server socket: %s\n", strerror(errno));
        close(sock->server_fd);
        pthread_mutex_destroy(&sock->socket_lock);
        return -4;
    }
    
    // Listen for connections
    if (listen(sock->server_fd, 5) == -1) {
        printf("ERROR: Failed to listen on server socket: %s\n", strerror(errno));
        close(sock->server_fd);
        unlink(sock->socket_path);
        pthread_mutex_destroy(&sock->socket_lock);
        return -5;
    }
    
    sock->client_fd = -1;
    
    printf("INFO: Unix socket server initialized at %s\n", sock->socket_path);
    return 0;
}

// Initialize Unix socket client (tile)
int unix_socket_init_client(unix_socket_comm_t* sock, int client_id) {
    if (!sock) {
        return -1;
    }
    
    memset(sock, 0, sizeof(unix_socket_comm_t));
    
    // Initialize mutex
    if (pthread_mutex_init(&sock->socket_lock, NULL) != 0) {
        printf("ERROR: Failed to initialize socket mutex\n");
        return -2;
    }
    
    // Create socket
    sock->client_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock->client_fd == -1) {
        printf("ERROR: Failed to create client socket: %s\n", strerror(errno));
        pthread_mutex_destroy(&sock->socket_lock);
        return -3;
    }
    
    // Set up server address (connect to C0 master socket)
    memset(&sock->server_addr, 0, sizeof(sock->server_addr));
    sock->server_addr.sun_family = AF_UNIX;
    snprintf(sock->socket_path, sizeof(sock->socket_path), "%s_0.sock", INTERRUPT_SOCKET_PATH_BASE);
    strcpy(sock->server_addr.sun_path, sock->socket_path);
    
    sock->server_fd = -1;
    
    printf("INFO: Unix socket client initialized for tile %d\n", client_id);
    return 0;
}

// Send IRQ via Unix socket
int unix_socket_send_irq(unix_socket_comm_t* sock, interrupt_request_t* irq) {
    if (!sock || !irq) {
        return -1;
    }
    
    char buffer[INTERRUPT_SOCKET_BUFFER_SIZE];
    int serialized_size = serialize_irq(irq, buffer, sizeof(buffer));
    if (serialized_size <= 0) {
        printf("ERROR: Failed to serialize IRQ\n");
        return -2;
    }
    
    pthread_mutex_lock(&sock->socket_lock);
    
    int fd = (sock->client_fd != -1) ? sock->client_fd : sock->server_fd;
    if (fd == -1) {
        printf("ERROR: No valid socket for sending\n");
        pthread_mutex_unlock(&sock->socket_lock);
        return -3;
    }
    
    ssize_t bytes_sent = send(fd, buffer, serialized_size, MSG_NOSIGNAL);
    pthread_mutex_unlock(&sock->socket_lock);
    
    if (bytes_sent == -1) {
        printf("ERROR: Failed to send IRQ: %s\n", strerror(errno));
        return -4;
    }
    
    if (bytes_sent != serialized_size) {
        printf("WARNING: Partial send: %zd/%d bytes\n", bytes_sent, serialized_size);
        return -5;
    }
    
    printf("DEBUG: Sent IRQ: %s from tile %d (%zd bytes)\n", 
           get_irq_type_name(irq->type), irq->source_tile, bytes_sent);
    
    return 0;
}

// Receive IRQ via Unix socket
int unix_socket_receive_irq(unix_socket_comm_t* sock, interrupt_request_t* irq) {
    if (!sock || !irq) {
        return -1;
    }
    
    char buffer[INTERRUPT_SOCKET_BUFFER_SIZE];
    
    pthread_mutex_lock(&sock->socket_lock);
    
    int fd = (sock->client_fd != -1) ? sock->client_fd : sock->server_fd;
    if (fd == -1) {
        printf("ERROR: No valid socket for receiving\n");
        pthread_mutex_unlock(&sock->socket_lock);
        return -3;
    }
    
    ssize_t bytes_received = recv(fd, buffer, sizeof(buffer), 0);
    pthread_mutex_unlock(&sock->socket_lock);
    
    if (bytes_received == -1) {
        printf("ERROR: Failed to receive IRQ: %s\n", strerror(errno));
        return -4;
    }
    
    if (bytes_received == 0) {
        printf("INFO: Socket closed by peer\n");
        return -5;
    }
    
    // Deserialize IRQ
    if (deserialize_irq(buffer, irq) != 0) {
        printf("ERROR: Failed to deserialize IRQ\n");
        return -6;
    }
    
    printf("DEBUG: Received IRQ: %s (%zd bytes)\n", 
           get_irq_type_name(irq->type), bytes_received);
    
    return 0;
}

// Clean up Unix socket
int unix_socket_cleanup(unix_socket_comm_t* sock) {
    if (!sock) {
        return -1;
    }
    
    pthread_mutex_lock(&sock->socket_lock);
    
    if (sock->client_fd != -1) {
        close(sock->client_fd);
        sock->client_fd = -1;
    }
    
    if (sock->server_fd != -1) {
        close(sock->server_fd);
        sock->server_fd = -1;
    }
    
    // Remove socket file if it exists
    if (sock->socket_path[0] != '\0') {
        unlink(sock->socket_path);
    }
    
    pthread_mutex_unlock(&sock->socket_lock);
    pthread_mutex_destroy(&sock->socket_lock);
    
    printf("INFO: Unix socket cleaned up\n");
    return 0;
}

// Utility functions

// Get socket path for entity
const char* get_socket_path(int entity_id) {
    static char path[256];
    snprintf(path, sizeof(path), "%s_%d.sock", INTERRUPT_SOCKET_PATH_BASE, entity_id);
    return path;
}

// Set socket timeout
int set_socket_timeout(int sockfd, int timeout_ms) {
    struct timeval timeout;
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;
    
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        printf("ERROR: Failed to set receive timeout: %s\n", strerror(errno));
        return -1;
    }
    
    if (setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
        printf("ERROR: Failed to set send timeout: %s\n", strerror(errno));
        return -2;
    }
    
    return 0;
}

// Serialize IRQ to buffer
int serialize_irq(interrupt_request_t* irq, char* buffer, size_t buffer_size) {
    if (!irq || !buffer || buffer_size < sizeof(interrupt_request_t)) {
        return -1;
    }
    
    // Simple serialization - just copy the struct
    // In a real implementation, this should handle endianness, alignment, etc.
    memcpy(buffer, irq, sizeof(interrupt_request_t));
    return sizeof(interrupt_request_t);
}

// Deserialize IRQ from buffer
int deserialize_irq(const char* buffer, interrupt_request_t* irq) {
    if (!buffer || !irq) {
        return -1;
    }
    
    // Simple deserialization - just copy the struct
    // In a real implementation, this should handle endianness, alignment, etc.
    memcpy(irq, buffer, sizeof(interrupt_request_t));
    return 0;
}

// Print communication statistics
void interrupt_comm_print_statistics(interrupt_communication_t* icomm) {
    if (!icomm) return;
    
    printf("\n=== Communication Statistics (Entity %d) ===\n", icomm->entity_id);
    printf("Method: %s\n", 
           icomm->method == COMM_METHOD_UNIX_SOCKET ? "Unix Socket" : "Other");
    printf("Role: %s\n", icomm->is_server ? "Server" : "Client");
    printf("Messages sent: %lu\n", icomm->messages_sent);
    printf("Messages received: %lu\n", icomm->messages_received);
    printf("Send failures: %lu\n", icomm->send_failures);
    printf("Receive failures: %lu\n", icomm->receive_failures);
    printf("Bytes sent: %lu\n", icomm->bytes_sent);
    printf("Bytes received: %lu\n", icomm->bytes_received);
    printf("===============================================\n\n");
}

// Reset communication statistics
void interrupt_comm_reset_statistics(interrupt_communication_t* icomm) {
    if (!icomm) return;
    
    icomm->messages_sent = 0;
    icomm->messages_received = 0;
    icomm->send_failures = 0;
    icomm->receive_failures = 0;
    icomm->bytes_sent = 0;
    icomm->bytes_received = 0;
    
    printf("INFO: Communication statistics reset for entity %d\n", icomm->entity_id);
}

// Error string function
const char* interrupt_comm_strerror(int error_code) {
    switch (error_code) {
        case 0: return "Success";
        case -1: return "Invalid argument";
        case -2: return "Initialization failed";
        case -3: return "Socket creation failed";
        case -4: return "Bind/Connect failed";
        case -5: return "Send/Receive failed";
        case -6: return "Serialization failed";
        default: return "Unknown error";
    }
} 