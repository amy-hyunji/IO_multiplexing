#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <netinet/ip.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <sys/syscall.h>

#include "tc_malloc.h"
#include "client.h"
#define MAXBUF 4096

pid_t gettid() { return syscall(__NR_gettid); }

/* for spinlock */
pthread_spinlock_t client_spinlock;

int client_spinlock_init(){
	pthread_spin_init(&client_spinlock, 0);
	return 0;
}

int fd_read (int fd, void *dbuf, size_t n){
    ssize_t rd = 0;
    size_t tr = 0;
    do {
        rd = pread(fd, dbuf, n, tr);
        if (rd<0) return -1;
        if (rd==0) break;
        tr += rd;
    }while (tr < n);
    return tr;
}

void* connection(void *arg){

	c_input_T c_arg = (c_input_T) arg;
	int numRequest = c_arg->request_num;
	int numThreads = c_arg->thread_num;
	char *searchWord = c_arg->search_word;
	struct sockaddr_in* server_address = c_arg->server_addr;

	int readnum = 0;
	int temp_num = 0;
	int client_fd;
	char buf[MAXBUF] = "";
	char s_buf[MAXBUF] = "";
	char s_input[MAXBUF] = "";

	//tcmalloc
	tc_thread_init();



	for (int i=0; i<numRequest; i++){
		//printf("%lu) working on request: %d\n", (unsigned long)gettid(), i);

		if ((client_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
			perror("Error on Socket");
			exit(-1);
		}

		if (connect(client_fd, (struct sockaddr*) server_address, sizeof(struct sockaddr))==-1){
			perror("Error on Connect");
			exit(-1);
		}

		///make protocol format///
		//pthread_spin_lock(&client_spinlock);
		protocol_T p_output = (protocol_T) tc_malloc(strlen(searchWord)+1+8);
		//pthread_spin_unlock(&client_spinlock);
		
		p_output->total_length = strlen(searchWord)+1+8;
		p_output->MSG_type = 16;
		p_output->payload = (char *)p_output+8;
		memcpy((char *)p_output+8, searchWord, strlen(searchWord)+1);

		write(client_fd, (char *)p_output, 8+strlen(searchWord)+1);

		//READ return from server//
		readnum = 0;
		while (readnum<4){
			temp_num = read(client_fd, s_input, sizeof(s_input));
			readnum += temp_num;
		}
		//printf("%lu) total_length: %u\n", (unsigned long)gettid(), ((protocol_T)s_input)->total_length);
		int _total_length = ((protocol_T)s_input)->total_length;

		while (readnum<8){
			temp_num = read(client_fd, s_input+readnum, sizeof(s_input));
			readnum += temp_num;
		}
		//printf("%lu) MSG: %u\n", (unsigned long)gettid(), ((protocol_T)s_input)->MSG_type);
		int _MSG = ((protocol_T)s_input)->MSG_type;

		if (_MSG == 32){
			printf("ERROR: file is not in server\n");
		}
		else {
			while (readnum<_total_length){
				temp_num = read(client_fd, s_input+readnum, sizeof(s_input));
				readnum += temp_num;
			}
			
				
			//printf("%lu) ARRIVED!\n", (unsigned long)gettid());
			

		}
		close(client_fd);
		tc_free(p_output);
	}
	tc_free(c_arg);
	return 0;
}


int main (int argc, char** argv){
	
	// divide the inputs
	int server_port, numThreads, numRequest;
	char *server_ip, *searchWord;
	server_ip = argv[1];
	server_port = atoi(argv[2]);
	numThreads = atoi(argv[3]);
	numRequest = atoi(argv[4]);
	searchWord = argv[5];

	//tcmalloc
	client_spinlock_init();
	tc_central_init();
	tc_thread_init();
	
	struct sockaddr_in *server_address = tc_malloc(sizeof(struct sockaddr_in));
	//memset (&server_address, 0, sizeof(server_address));
	c_input_T c_arg = ((c_input_T)tc_malloc(sizeof(struct c_input)));

    //list of threads
	pthread_t thread_list[numThreads];


	// change domain name -> IP addr 
	struct hostent* host = gethostbyname(server_ip);
	if (host == NULL) exit(-1);
	//host->h_addrtype = AF_INET; //*****
	struct in_addr *IP_addr = (struct in_addr*) host->h_addr;
	
	server_address->sin_family = AF_INET;
	server_address->sin_port = htons(server_port);
	server_address->sin_addr = *IP_addr;
	bzero(&(server_address->sin_zero), 8);

	c_arg->thread_num = numThreads;
	c_arg->request_num = numRequest;
	c_arg->search_word = (char*)tc_malloc(strlen(searchWord)+1);
	memcpy(c_arg->search_word, searchWord, strlen(searchWord)+1);
	c_arg->server_addr = server_address;

	// main process generates threads as the number of request from the arguments
	for (int i=0; i<numThreads; i++){
		if (pthread_create(&thread_list[i], NULL, connection, (void *)c_arg)){
			printf("FAIL: pthread_create\n");
			return -1;
		}
	}

	for (int i=0; i<numThreads; i++){
		if (pthread_join(thread_list[i], NULL)){
			printf("FAIL: pthread_join\n");
			return -1;
		}
	}
}
