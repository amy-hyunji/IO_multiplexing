//
// Created by 이현지 on 28/09/2019.
//
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netdb.h>
#include <sys/syscall.h>

#include "client2.h"
#include "tc_malloc.h"

#define MAXCHAR 1024 // length of buffer when reading the file
#define HASHLEN 4096 // length of hash table
#define MAXARRAY 1024 // how many times word appear in folder
#define HASH_MULTIPLIER 665599
#define MAXBUF 4096 

int fd_read(int fd, void *dbuf, size_t n, int len){
	int rd = 0;
	int tr = 0;
	do {
		rd = read(fd, dbuf+tr, n);
		if (rd<0) return -1;
		if (rd==0) break;
		tr += rd;
	}while (tr < n);
	return tr;
}

void *connection(void *arg){
	
	c_input_T c_arg = (c_input_T) arg;
	int numRequest = c_arg->request_num;
	int numThreads = c_arg->thread_num;
	struct sockaddr_in* server_address = c_arg->server_addr;
	char* no_need = (char *)calloc(1, MAXCHAR);
	char* searchWord = (char*)calloc(1, MAXCHAR);
	
	int readnum= 0;
	int temp_num = 0;
	int client_fd;
	char s_buf[MAXBUF] = "";
	char buf[MAXBUF] = "";
	char s_input[MAXBUF] = "";

	tc_thread_init();
	while (1){
		printf("%s> ", "mininet>");
		scanf("%s %s", no_need, searchWord);

		if ((client_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1){
			perror("Error on Socket");
			exit(-1);
		}

		if (connect(client_fd, (struct sockaddr*)server_address, sizeof(struct sockaddr)) == -1){
			perror("Error on Connect");
			exit(-1);
		}

		///////WRITE to server////////

		//structure for protocol
		protocol_T p_output = (protocol_T) tc_malloc(strlen(searchWord)+1+8);
		p_output->total_length = strlen(searchWord)+1+8;
		p_output->MSG_type = 16;
		p_output->payload = (char*) p_output+8;
		memcpy((char*)p_output+8, searchWord, strlen(searchWord)+1);

		/*
		printf("p_output->total_length: %u\n", p_output->total_length);
		printf("p_output->MSG_type: %u\n", p_output->MSG_type);
		printf("p_output->payload: %s\n", (char*)p_output+8);
		*/

		write(client_fd, (char*) p_output, 8+strlen(searchWord)+1);
		
		///////READ return from server/////////
		readnum = 0;
		while (readnum<4){
			temp_num = read(client_fd, s_input, sizeof(s_input));
			readnum += temp_num;
		}
		//printf("total_length: %u\n", ((protocol_T)s_input)->total_length);
		int _total_length = ((protocol_T)s_input)->total_length;

		while (readnum<8){
			temp_num = read(client_fd, s_input+readnum, sizeof(s_input));
			readnum += temp_num;
		}
		//printf("MSG: %u\n", ((protocol_T)s_input)->MSG_type);
		int _MSG = ((protocol_T)s_input)->MSG_type;
		
		if (_MSG == 32){
			printf("ERROR: file is not in server\n");
		}
		else {
			while (readnum < _total_length){
				temp_num = read(client_fd, s_input+readnum, sizeof(s_input));
				readnum += temp_num;
			}
			
			printf("%s", s_input+8);
		}
		
		close(client_fd);
		tc_free(p_output);
	}
	tc_free(c_arg);
	return 0;
}

int main(int argc, char *argv[]){
   
	 // input format
	 // “./client2 [server_ip] [word_to_search]”, (e.g., ./client 127.0.0.1 8080)

	 int server_port, numThreads, numRequest;
	 char *server_ip;
	 char *output = argv[0];
	 output = strtok(output, "./");
	 server_ip = argv[1];
	 server_port = atoi(argv[2]);
	 numThreads = 1;
	 numRequest = 1;
	 if (numThreads != 1 | numRequest != 1){
		printf("number of Thread and number of Request should be 1 in this client\n");
		exit(-1);
	 }

	 /* 
	 printf("server_ip: %s\n", server_ip);
	 printf("server port: %d\n", server_port);
	 printf("threadnum: %d\n", numThreads);
	 printf("request_num: %d\n", numRequest);
	 */

	 pthread_t thread_list[numThreads]; 
	 //printf("working on...\n");
	 tc_central_init();
	 //printf("INITIALIZE tcmalloc central\n");
	 tc_thread_init();
	 //printf("INITIALIZE tcmalloc thread\n");

	 struct sockaddr_in* server_address = tc_malloc(sizeof(struct sockaddr_in));
	 c_input_T c_arg = ((c_input_T)tc_malloc(sizeof (struct c_input)));
	 

	 struct hostent* host = gethostbyname(server_ip);
	 if (host == NULL) exit(-1);
	 //host->h_addrtype = AF_INET;
	 struct in_addr *IP_addr = (struct in_addr*) host->h_addr;

	 server_address->sin_family = AF_INET;
	 server_address->sin_port = htons(server_port);
	 server_address->sin_addr = *IP_addr;
	 bzero(&(server_address->sin_zero), 0);

	 c_arg->thread_num = numThreads;
	 c_arg->request_num = numRequest;
	 c_arg->server_addr = server_address;

	 if (pthread_create(&thread_list[0], NULL, connection, (void *)c_arg)){
		 printf("FAIL: pthread_create\n");
		 return -1;
	 }
	 //printf("create new thread\n");

	 if (pthread_join(thread_list[0], NULL)){
		printf("FAIL: pthread_join\n");
		return -1;
	 }

	 return 0;
}
