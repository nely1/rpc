#include "rpc.h"


#ifndef RPC_HELPER_H
#define RPC_HELPER_H

// -------------------------------------------CLIENT SIDE HELPERS-------------------------------------------------------

// Creates the client socket and sets up connection with server
void setup_client_socket(rpc_client *client, char *addr, int port);

// Marshalls the data that will be used for rpc_find and store it in the buffer
void marshall_find(char *buffer, char *name);

// Demarshalls the handle that is received from the server
rpc_handle *demarshall_handle(char *buffer);

// Marshalls the data that will be used for rpc_call
void marshall_call(char *buffer, rpc_handle *handle, rpc_data *payload);

// Demarshalls the data that is received from the server
rpc_data *demarshall_call_result(char *buffer);



// -------------------------------------------SERVER SIDE HELPERS-------------------------------------------------------

// Creates the server socket, binds and listens
void setup_server_socket(rpc_server *server, int port);

// Finds the handle that corresponds to the name received from the client
rpc_handle *find_function_handle(char *name, rpc_server *state);

// Demarshalls any data of type rpc_data
rpc_data *demarshall_data(char buffer[], size_t *buffer_pointer);

// Marshalls the handle that is found and stores it in the buffer to send to the client
void marshall_handle(rpc_handle *handle, char buffer[]);

// Demarshalls the handle that was received from the client in an rpc_call
rpc_handle *demarshall_call_handle(char buffer[], size_t *buffer_pointer);

// Marshalls the data received from the handle call and sends to client
void marshall_call_result(char buffer[], rpc_data *result);

// Checks if the name of a function is between ASCII 32 and 36
int isAscii(char *name, int length);

// -------------------------------------------GENERAL HELPERS-----------------------------------------------------------

// Converts a value to network byte order. Usually used for int64_t or size_t variables
void *htonll(void *value, size_t num_bytes);

// Converts a value to host byte order. Usually used for int64_t or size_t variables
void *ntohll(void *value, size_t num_bytes);

// Tests whether the system is big endian or small endian
int64_t test_big_endian();

// Swaps the byte order of a certain variable
void swap_bytes(void *value, size_t num_bytes);

// Frees a handle
void free_handle(rpc_handle *handle);

#endif
