#define _POSIX_C_SOURCE 200112L
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include "rpc.h"
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

rpc_server *rpc_init_server(int port) {
    rpc_server *server = NULL;
    if (port) {

        server = (rpc_server*)malloc(sizeof(rpc_server));
        server->port = port;
        server->num_handles = 0;
        server->handles = (rpc_handle**)malloc(sizeof(rpc_handle*) * FUNCTION_LIST_SIZE);
        server->max_handles_size = FUNCTION_LIST_SIZE;
        server->listenfd = 0;
        server->connfd = 0;

        setup_server_socket(server, port);
    }
    return server;
}


int rpc_register(rpc_server *srv, char *name, rpc_handler handler) {
    
    if (!srv || !name) {
        return -1;
    }

    
    rpc_handle *handle = (rpc_handle *)malloc(sizeof(rpc_handle));
    handle->name = (char*)malloc(sizeof(char) * strlen(name) + 1);
    strcpy(handle->name, name);
    handle->handler = handler;

    if (strlen(name) == 0) {
        return -1;
    }

    if (!isAscii(name, strlen(name))) {
        return -1;
    }

    // Replace the handle that has the same name as the handle being registered
    for (int i  = 0; i < srv->num_handles; i++) {
        if (strcmp(srv->handles[i]->name ,handle->name) == 0) {
            free(srv->handles[i]->name);
            free(srv->handles[i]);
            srv->handles[i] = handle;
        }
    }
    srv->handles[srv->num_handles] = handle;
    srv->num_handles += 1;

    if (srv->num_handles == srv->max_handles_size) {
        srv->max_handles_size *= 2;
        srv->handles = (rpc_handle**)realloc(srv->handles, srv->max_handles_size);
    }
    if (handle) {
        return 1;
    }

    return -1;
}


void rpc_serve_all(rpc_server *srv) {
        while (1) {
            // Accept connections
            memset(srv->buffer, 0, BUFFER_SIZE);
            struct sockaddr_storage client_addr;
            socklen_t client_addr_size = sizeof(client_addr);
            srv->connfd = accept(srv->listenfd, (struct sockaddr*)&client_addr, &client_addr_size);

            if (srv->connfd < 0) {
                fprintf(stderr, "Failed to accept connection\n");
                exit(EXIT_FAILURE);
            }


            if (read(srv->connfd, srv->buffer, sizeof(srv->buffer)) < 0) {
                fprintf(stderr, "Could not read from client\n");
            };

            // Determine the request type and act accordingly: find = 0, call = 1
            int64_t request_type = -1;
            memcpy(&request_type, srv->buffer, sizeof(int64_t));
            ntohll(&request_type, sizeof(int64_t));
            
           
            rpc_handle *handle = NULL;
            rpc_data *result = NULL;
            rpc_data *payload = NULL;
            rpc_data *data = NULL;
            size_t buffer_pointer = 0;

            if (request_type == 0) {
                data = demarshall_data(srv->buffer, &buffer_pointer);
                handle = find_function_handle((char*)data->data2, srv);
                marshall_handle(handle, srv->buffer);

            }
            else if (request_type == 1) {
                handle = demarshall_call_handle(srv->buffer, &buffer_pointer);
                payload = demarshall_data(srv->buffer, &buffer_pointer);
                result = (handle->handler)(payload);
                marshall_call_result(srv->buffer, result);
                free_handle(handle);
            }

            if (write(srv->connfd, srv->buffer, sizeof(srv->buffer)) < 0) {
                fprintf(stderr, "Could not write to client\n");
                exit(EXIT_FAILURE);
            }

       
        rpc_data_free(result);
        rpc_data_free(data);
        rpc_data_free(payload);
        close(srv->connfd);
    }
}

rpc_client *rpc_init_client(char *addr, int port) {

    rpc_client *client = NULL; 

    if (addr && port) {
        client = (rpc_client*)malloc(sizeof(rpc_client));
        client->addr = malloc(strlen(addr) + 1);
        strcpy(client->addr, addr);
        client->port = port;

    }
    if (client) {
        return client;
    }

    return NULL;
}


rpc_handle *rpc_find(rpc_client *cl, char *name) {

    if (!cl || !name) {
        return NULL;
    }

    setup_client_socket(cl, cl->addr, cl->port);
    marshall_find(cl->buffer, name);
    
    
    if (write(cl->connfd, cl->buffer, sizeof(cl->buffer)) < 0) {
        fprintf(stderr, "Could not send to server\n");
        exit(EXIT_FAILURE);
    }

    memset(cl->buffer, 0, BUFFER_SIZE);


    if (read((cl->connfd), cl->buffer, sizeof(cl->buffer)) < 0) {
        fprintf(stderr, "Could not read from server\n");
        exit(EXIT_FAILURE);
    }

    rpc_handle* handle = demarshall_handle(cl->buffer);

    return handle;
}


rpc_data *rpc_call(rpc_client *cl, rpc_handle *h, rpc_data *payload) {

    // Check for bad data
    if (!cl || !h || !payload) {
        
        return NULL;
    }
    if (payload->data2_len == 0 && payload->data2) {
        return NULL;
    }
    if (payload->data2_len > 0 && !payload->data2) {
        return NULL;
    }

    memset(cl->buffer, 0, BUFFER_SIZE);
    setup_client_socket(cl, cl->addr, cl->port);
    
    marshall_call(cl->buffer, h, payload);

    if (write(cl->connfd, cl->buffer, sizeof(cl->buffer)) < 0) {
        fprintf(stderr, "Could not send to server\n");
        exit(EXIT_FAILURE);
    }

    memset(cl->buffer, 0, sizeof(BUFFER_SIZE));

    if (read((cl->connfd), cl->buffer, sizeof(cl->buffer)) < 0) {
        fprintf(stderr, "Could not read from server\n");
        exit(EXIT_FAILURE);
    }

    rpc_data *result = demarshall_call_result(cl->buffer);    

    return result;
}

void rpc_close_client(rpc_client *cl) {
    if (!cl) {
        return;
    }
    else {
        free(cl->addr);
        free(cl);
    }
}

void rpc_data_free(rpc_data *data) {

    if (data == NULL) {
        return;
    }
     if (data->data2 != NULL) {
        free(data->data2);
    }
    free(data);
}


