LDFLAGS=-L. $(RPC_SYSTEM_A)
CC=cc
RPC_SYSTEM=rpc.o
TEST_SERVER_A=server.a
TEST_CLIENT_A=client.a
TEST_SERVER=rpc-server
TEST_CLIENT=rpc-client
RPC_HELPER=rpc_helper.o
RPC_SYSTEM_A=rpc.a

.PHONY: format all

all: $(RPC_SYSTEM_A) $(TEST_CLIENT) $(TEST_SERVER)

$(RPC_SYSTEM): rpc.c rpc.h 
	$(CC) -Wall -c -o $@ $<


$(RPC_HELPER): rpc_helper.c rpc_helper.h
	$(CC) -Wall -c -o $@ $< 

$(RPC_SYSTEM_A): $(RPC_SYSTEM) $(RPC_HELPER)
	ar rcs $(RPC_SYSTEM_A) $(RPC_SYSTEM) $(RPC_HELPER)


$(TEST_SERVER): $(TEST_SERVER_A) $(RPC_SYSTEM) $(RPC_HELPER)
	$(CC) -Wall -o $@ $< $(RPC_SYSTEM) $(RPC_HELPER)

$(TEST_CLIENT): $(TEST_CLIENT_A) $(RPC_SYSTEM) $(RPC_HELPER)
	$(CC) -Wall -o $@ $< $(RPC_SYSTEM) $(RPC_HELPER)




format:
	clang-format -style=file -i *.c *.h

clean:
	rm -f *.o $(TEST_SERVER) $(TEST_CLIENT) *.a
