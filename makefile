all: client1 client2 server

client1: client.c tc_malloc.c
	gcc -pthread -o client1 client.c tc_malloc.c

client2: client2.c tc_malloc.c
	gcc -pthread -o client2 client2.c tc_malloc.c

server: server.c tc_malloc.c threadpool.c
	gcc -pthread -o server server.c tc_malloc.c threadpool.c
