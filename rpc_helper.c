#define _POSIX_C_SOURCE 200112L  
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include "rpc_helper.h"
#define FUNCTION_LIST_SIZE 10
#define BUFFER_SIZE 100000



struct rpc_handle {
    /* Add variable(s) for handle */
    char *name;
    rpc_handler handler;
};

struct rpc_server {
    /* Add variable(s) for server state */
    int port;
    rpc_handle **handles;
    int num_handles;
    int max_handles_size;
    char buffer[BUFFER_SIZE];
    int connfd;
    int listenfd;
};

struct rpc_client {
    /* Add variable(s) for client state */
    char *addr;
    int port;
    int connfd;
    char buffer[BUFFER_SIZE];
};

// -------------------------------------------CLIENT SIDE HELPERS-------------------------------------------------------
void setup_client_socket(rpc_client *client, char *addr, int port) {
    
    int s;
    struct addrinfo hints, *res, *rp;
    char str_port[100];
    snprintf(str_port, sizeof(size_t), "%d",port);
    
    // Define socket
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET6;
    hints.ai_socktype = SOCK_STREAM;
    s = getaddrinfo(addr, str_port, &hints, &res);
    if (s < 0) {
        fprintf(stderr, "get addr info fail\n");
        exit(EXIT_FAILURE);
    }
    for (rp = res; rp != NULL; rp = rp->ai_next) {
    
            client->connfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
            if (client->connfd == -1) {
                continue;
            }
            if (connect(client->connfd, rp->ai_addr, rp->ai_addrlen) != -1) {
                break;
            }
            close(client->connfd);
    }
    free(res);

    if (client->connfd < 0) {
        fprintf(stderr, "No connection made\n");
        exit(EXIT_FAILURE);
    }
}



// memcpy code acknowledged in stackoverflow: https://stackoverflow.com/questions/15707933/how-to-serialize-a-struct-in-c
void marshall_find(char *buffer, char *name) {
    memset(buffer, 0, BUFFER_SIZE);
    
    // Store variables in the same structure as rpc_data
    int64_t data1 = 0;
    size_t data2_len = (size_t)(strlen(name) + 1);

    if (data2_len + sizeof(size_t) + sizeof(int64_t) > BUFFER_SIZE) {
        fprintf(stderr,  "Overlength error\n");
        exit(EXIT_FAILURE);
    }

    char *data2 = (char*)malloc(sizeof(char) * strlen(name) + 1);
    strcpy(data2, name);
    
    // Store the data into the buffer
    size_t buffer_pointer = 0;
    memcpy(buffer + buffer_pointer, htonll(&data1, sizeof(int64_t)), sizeof(int64_t));
    buffer_pointer += sizeof(int64_t);
    
    memcpy(buffer + buffer_pointer, htonll(&data2_len, sizeof(size_t)), sizeof(size_t));
    buffer_pointer += sizeof(size_t);
    
    strcpy(buffer + buffer_pointer, data2);
    
    free(data2);

}

rpc_handle *demarshall_handle(char *buffer) {
    // Read from the buffer and store the data in result
   
    int64_t success;
    size_t buffer_pointer = 0;
    memcpy(&success, buffer + buffer_pointer, sizeof(int64_t));
    ntohll(&success, sizeof(int64_t));

    if (success < 0) {
        return NULL;
    }

    buffer_pointer += sizeof(int64_t);
    
    size_t name_len = 0;
    size_t data2_len = 0;
    size_t handler_len = 0;
    
    rpc_handle *result = (rpc_handle*)malloc(sizeof(rpc_handle));

    memcpy(&data2_len, buffer + buffer_pointer, sizeof(size_t));
    ntohll(&data2_len, sizeof(size_t));

    buffer_pointer += sizeof(size_t);
    
    memcpy(&name_len, buffer + buffer_pointer, sizeof(size_t));
    ntohll(&name_len, sizeof(size_t));
    buffer_pointer += sizeof(size_t);

    result->name = (char*)malloc(sizeof(char) * name_len);
    strcpy(result->name, buffer + buffer_pointer);
    buffer_pointer += name_len;
    
    memcpy(&handler_len, buffer + buffer_pointer, sizeof(size_t));
    ntohll(&handler_len, sizeof(size_t));
    buffer_pointer += sizeof(size_t);
    
    memcpy(&result->handler, buffer + buffer_pointer, handler_len); 
    

    return result;
}


void marshall_call(char *buffer, rpc_handle *handle, rpc_data *payload) {

    
    int64_t request_type = 1;
        
    size_t handle_name_len = strlen(handle->name) + 1;
    size_t original_handle_name_len = handle_name_len;
    size_t handler_len = sizeof(handle->handler);
    size_t original_handler_len = handler_len;
    size_t original_payload2_len = payload->data2_len;
    size_t data2_len = sizeof(size_t) + handle_name_len + sizeof(size_t) + handler_len + sizeof(int64_t) +  sizeof(size_t) + payload->data2_len;

    if (data2_len + sizeof(int64_t) + sizeof(size_t) > BUFFER_SIZE) {
        fprintf(stderr, "Overlength error\n");
        exit(EXIT_FAILURE);
    }

    size_t buffer_pointer = 0;
    
    memcpy(buffer + buffer_pointer, htonll(&request_type, sizeof(int64_t)), sizeof(int64_t));
    buffer_pointer += sizeof(int64_t);

    memcpy(buffer + buffer_pointer, htonll(&data2_len, sizeof(size_t)), sizeof(size_t));
    buffer_pointer += sizeof(size_t);
    
    // Store handle in buffer
    memcpy(buffer + buffer_pointer, htonll(&handle_name_len, sizeof(size_t)), sizeof(size_t));
    buffer_pointer += sizeof(size_t);

    strcpy(buffer + buffer_pointer, handle->name);
    buffer_pointer += original_handle_name_len;
    
    memcpy(buffer + buffer_pointer, htonll(&handler_len, sizeof(size_t)), sizeof(size_t));
    buffer_pointer += sizeof(size_t);
    
    memcpy(buffer + buffer_pointer, &handle->handler, original_handler_len);
    buffer_pointer += original_handler_len;


    // Store payload in buffer
    int64_t data1 = (int64_t)payload->data1;
    memcpy(buffer + buffer_pointer, htonll(&data1, sizeof(int64_t)), sizeof(int64_t));
    buffer_pointer += sizeof(int64_t);
    
    memcpy(buffer + buffer_pointer, htonll(&payload->data2_len, sizeof(size_t)), sizeof(size_t));
    buffer_pointer += sizeof(size_t);
    
    if (original_payload2_len > 0 && payload->data2) {
        memcpy(buffer + buffer_pointer, payload->data2, original_payload2_len);
    }    
}

rpc_data *demarshall_call_result(char *buffer) {
    size_t buffer_pointer = 0;
    int64_t response_type = 0;

    memcpy(&response_type, buffer + buffer_pointer, sizeof(int64_t));
    buffer_pointer += sizeof(int64_t);
    ntohll(&response_type, sizeof(int64_t));
    
    if (response_type < 0) {
        return NULL;
    }

    size_t data2_len = 0;
    memcpy(&data2_len, buffer + buffer_pointer, sizeof(size_t));
    ntohll(&data2_len, sizeof(size_t));
    buffer_pointer += sizeof(size_t);


    rpc_data *result = (rpc_data*)malloc(sizeof(rpc_data));

    memcpy(&result->data1, buffer + buffer_pointer, sizeof(int64_t));
    ntohll(&result->data1, sizeof(int64_t));
    buffer_pointer += sizeof(int64_t);
    
    memcpy(&result->data2_len, buffer + buffer_pointer, sizeof(size_t));
    ntohll(&result->data2_len, sizeof(size_t));
   
    buffer_pointer += sizeof(size_t);
    
    if (result->data2_len <= 0) {

        result->data2 = NULL;
    }
    else {
        result->data2 = malloc(result->data2_len);
        memcpy(result->data2, buffer + buffer_pointer, result->data2_len);
    }
    
    return result;
}

// -------------------------------------------SERVER SIDE HELPERS-------------------------------------------------------
void setup_server_socket(rpc_server *server, int port) {
    // Define socket variables
    int re = 1, s;
    struct addrinfo hints, *res, *rp;
    char str_port[100];
    snprintf(str_port, sizeof(size_t), "%d",port);
    
    // Define socket
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET6;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;    
    
    s = getaddrinfo(NULL, str_port, &hints, &res);
    if (s < 0) {
        fprintf(stderr, "%s\n", gai_strerror(s));
        exit(EXIT_FAILURE);
    }
    
    for (rp = res; rp != NULL; rp = rp->ai_next) {
        if (rp->ai_family == AF_INET6) {
            server->listenfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
            if (server->listenfd < 0) {
                continue;
            }
            else {
                break;
            }
        }
    }
    
    if (server->listenfd < 0) {
        fprintf(stderr, "Socket creation failed\n");
        exit(EXIT_FAILURE);
    }
    
    if (setsockopt(server->listenfd, SOL_SOCKET, SO_REUSEADDR, &re, sizeof(re)) < 0) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    
    // Bind and listen
    if (bind(server->listenfd, res->ai_addr, res->ai_addrlen) != 0) {
        fprintf(stderr, "Bind failed\n");
        exit(EXIT_FAILURE);
    }
    
    
    if (listen(server->listenfd, 10) < 0) {
        fprintf(stderr, "Listen failed\n");
        exit(EXIT_FAILURE);
    };
    freeaddrinfo(res);
}

rpc_handle *find_function_handle(char *name, rpc_server *state) {
    for (int i = 0; i < state->num_handles; i++) {
        if (strcmp(name, state->handles[i]->name) == 0) {
            return state->handles[i];
        }
    }
    return NULL;
}

rpc_data *demarshall_data(char buffer[], size_t *buffer_pointer) {

    rpc_data *data = (rpc_data*)malloc(sizeof(rpc_data));
    
    memcpy(&data->data1, buffer + *buffer_pointer, sizeof(int64_t));
    ntohll(&data->data1, sizeof(int64_t));
    *buffer_pointer += sizeof(int64_t);

    memcpy(&data->data2_len, buffer + *buffer_pointer, sizeof(size_t));
    ntohll(&data->data2_len, sizeof(size_t));
    *buffer_pointer += sizeof(size_t);

    
    if (data->data2_len <= 0 ) {
        data->data2 = NULL;
    }
    else {
        data->data2 = malloc(data->data2_len);
        memcpy(data->data2, buffer + *buffer_pointer, data->data2_len);
    }

    return data;
}


void marshall_handle(rpc_handle *handle, char buffer[]) {
    
    // Store the handle in the buffer
    // Format of buffer after storing handle: [data1: response type, data2_len, data2: handle info]
    
    memset(buffer, 0, BUFFER_SIZE);

    if (!handle) {
        int64_t error = -1;
        memcpy(buffer, htonll(&error, sizeof(int64_t)), sizeof(int64_t));

    }
    else {

        int64_t data1 = 0;
        size_t buffer_pointer = 0;
        size_t name_len = strlen(handle->name) + 1;
        size_t original_name_len = name_len;
        size_t handler_len = sizeof(handle->handler);
        size_t original_handler_len = handler_len;

        memcpy(buffer + buffer_pointer, htonll(&data1, sizeof(int64_t)), sizeof(int64_t));
        buffer_pointer += sizeof(int64_t);
        
        size_t data2_len = sizeof(size_t) + name_len + sizeof(size_t) + handler_len;

        if (data2_len + sizeof(int64_t) + sizeof(size_t) > BUFFER_SIZE) {
            fprintf(stderr, "Overlength error\n");
            exit(EXIT_FAILURE);
        }
        
        memcpy(buffer + buffer_pointer, htonll(&data2_len, sizeof(size_t)), sizeof(size_t));
        buffer_pointer += sizeof(size_t);
        
        memcpy(buffer + buffer_pointer, htonll(&name_len, sizeof(size_t)), sizeof(size_t));
        buffer_pointer += sizeof(size_t);
        
        strcpy(buffer + buffer_pointer, handle->name);
        buffer_pointer += original_name_len;
        
        memcpy(buffer + buffer_pointer,htonll(&handler_len, sizeof(size_t)), sizeof(size_t));
        buffer_pointer += sizeof(size_t);
        
        memcpy(buffer + buffer_pointer, &handle->handler, original_handler_len);
        
    }
}

rpc_handle *demarshall_call_handle(char buffer[], size_t *buffer_pointer) {

    rpc_handle *handle = (rpc_handle*)malloc(sizeof(rpc_handle));
    size_t handle_name_len = 0;
    size_t handler_len = 0;
    
    int64_t request_type = -1;
    memcpy(&request_type, buffer + *buffer_pointer, sizeof(int64_t));
    ntohll(&request_type, sizeof(int64_t));
    *buffer_pointer += sizeof(int64_t);
    
    size_t data2_len = 0;
    memcpy(&data2_len, buffer + *buffer_pointer, sizeof(size_t));
    ntohll(&request_type, sizeof(size_t));
    *buffer_pointer += sizeof(size_t);

    memcpy(&handle_name_len, buffer + *buffer_pointer, sizeof(size_t));
    ntohll(&handle_name_len, sizeof(size_t));
    *buffer_pointer += sizeof(size_t);
    
    handle->name = malloc(handle_name_len);
    strcpy(handle->name, buffer + *buffer_pointer);
    *buffer_pointer += handle_name_len;
    
    memcpy(&handler_len, buffer + *buffer_pointer, sizeof(size_t));
    ntohll(&handler_len, sizeof(size_t));
    *buffer_pointer += sizeof(size_t);
    
    memcpy(&handle->handler, buffer + *buffer_pointer, handler_len);
    *buffer_pointer += handler_len;

    return handle;
}

void marshall_call_result(char buffer[], rpc_data *result) {
    // Format of buffer after storing result: [data1: response_type, data2_len, data2: result info]

    memset(buffer, 0, BUFFER_SIZE);
    size_t buffer_pointer = 0;
    int64_t response_type = 0;
    
    // Check for bad data from result
    if ((!result) || (result->data2_len == 0 && result->data2) || (result->data2_len > 0 && !result->data2)) {
        response_type = -1;
    }
    int64_t original_response_type = response_type;
    
    memcpy(buffer + buffer_pointer, htonll(&response_type, sizeof(int64_t)), sizeof(int64_t));
    buffer_pointer += sizeof(int64_t);
    
    // If response is successful then store everything else in the buffer
    if (original_response_type == 0) {
        size_t original_result_data2_len = result->data2_len;
    
        size_t data2_len = sizeof(int64_t) + sizeof(size_t) + result->data2_len;

        if (data2_len + sizeof(int) + sizeof(size_t) > BUFFER_SIZE) {
            fprintf(stderr, "Overlength error\n");
            exit(EXIT_FAILURE);
        }
        
        memcpy(buffer + buffer_pointer, htonll(&data2_len, sizeof(size_t)), sizeof(size_t));
        buffer_pointer += sizeof(size_t);
        
        int64_t data1 = (int64_t)result->data1;
        memcpy(buffer + buffer_pointer, htonll(&data1, sizeof(int64_t)), sizeof(int64_t));
        buffer_pointer += sizeof(int64_t);
        memcpy(buffer + buffer_pointer, htonll(&result->data2_len, sizeof(size_t)), sizeof(size_t));
        buffer_pointer += sizeof(size_t);
        
        
        if (original_result_data2_len > 0) {
            memcpy(buffer + buffer_pointer, result->data2, original_result_data2_len);
        }       
    }
}

// -------------------------------------------GENERAL HELPERS-----------------------------------------------------------

void *htonll(void* value, size_t num_bytes) {

    if (test_big_endian()) {
        return value;
    }

    swap_bytes(value, num_bytes);
    return value;
}

void *ntohll(void *value, size_t num_bytes) {

    if (test_big_endian()) {
        return value;
    }

    swap_bytes(value, num_bytes);
    return value;
}

int64_t test_big_endian() {

    int64_t test = 1;
    char* bytes =  (char*)&test;

    if (bytes[0] == 1) {
        return 0;
    }

    return 1;
}

// Code acknowledged in stackoverflow: https://stackoverflow.com/questions/28203943/byte-swapping-in-bit-wise-operations
void swap_bytes(void *value, size_t num_bytes) {

    char *bytes = (char*)value;

    for (int i = 0; i < num_bytes / 2; i++) {

        char temp = bytes[i];
        bytes[i] = bytes[num_bytes - 1 - i];
        bytes[num_bytes - 1 -i] = temp;
    }

}

int isAscii(char *name, int length) {

    for (int i = 0; i < length; i++) {
        if (name[i] < 32 || name[i] > 126) {
            return 0;
        }
    }

    return 1;
}

void free_handle(rpc_handle *handle) {
    if (handle) {
        free(handle);
    }
}