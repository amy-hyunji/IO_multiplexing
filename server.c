//SERVER.C//

#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <string.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <spawn.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/syscall.h>
#include <pthread.h>

#include "tc_malloc.h"
#include "threadpool.h"
#include "server.h"

#define TRUE 1
#define FALSE 0
#define MAXBUF 100
#define MAXCHAR 1024
#define HASHLEN 4096
#define MAXARRAY 1024
#define HASH_MULTIPLIER 665599

pid_t gettid() {return syscall(__NR_gettid);}

/*
	for spinlock
*/
pthread_spinlock_t spinlock;

int spinlock_init(){
	pthread_spin_init(&spinlock, 0);
	return 0;
}


/*--------------------------------------------------------------------*/

struct HashTable {
	Word_T array [HASHLEN];
};

struct Array_SavePoints {
	SavePoints_T array [MAXARRAY];
};

struct Word {
	char* word;
	int freq;
	Word_T next_word;
	Array_SavePoints_T array;
};

struct SavePoints {
	char* filename;
	int line;
	SavePoints_T next_point;
};

// contains information of hashtable - duplicated ones
struct DB {
	HashTable_T ht_word;
	int curArrSize; // length of hashtable
};

/*--------------------------------------------------------------------*/

DB_T CreateWordDB(void) {
	DB_T d;
	d = (DB_T) tc_malloc(sizeof(struct DB));
	if (d == NULL) {
		fprintf (stderr, "Cannot allocate a memory for new DB\n");
		return NULL;
	}
	d->ht_word = (HashTable_T) CreateHashTable();
	d->curArrSize = HASHLEN;
	return d;
}

HashTable_T CreateHashTable(void) {
	HashTable_T ht;

	ht = (HashTable_T) tc_malloc(sizeof(struct HashTable));
	if (ht == NULL) {
		fprintf (stderr, "Cannot allocate a memory for new HashTable\n");
		return NULL;
	}
	return ht;
}

static int hash_function (const char *word, int iBucketCount){
	int i;
	unsigned int uiHash = 0U;
	for (i=0; word[i] != '\0'; i++) {
		uiHash = uiHash * (unsigned int)HASH_MULTIPLIER + (unsigned int)word[i];
	}
	return (int)(uiHash % (unsigned int)iBucketCount);
}

int CheckExistence (HashTable_T ht, char *word, char *filename, int line) {
	int h_word = hash_function(word, HASHLEN);
	Word_T check = NULL;
	for (check = ht->array[h_word]; check != NULL; check = check->next_word){
		if (strcmp(check->word, word) == 0) { // word exists
			SavePoints_T sp = AddSavePoints(word, filename, line);
			SavePoints_T doc = check->array->array[0];
			while (doc->next_point != NULL) { doc = doc -> next_point; }
			doc->next_point = sp;
			check->freq = check->freq + 1;
			return 1;
		}
	}
	// add word
	Word_T new_word = AddNewWord(word);
	SavePoints_T sp = AddSavePoints(word, filename, line);
	new_word->array->array[0] = sp;
	new_word->next_word = ht->array[h_word];
	ht->array[h_word] = new_word;
	return 0;
}

Word_T AddNewWord(char *add_word){
	Word_T n_word = (Word_T) tc_malloc(sizeof(struct Word));
	n_word->word = (char *) tc_malloc(MAXCHAR);
	strcpy(n_word->word, add_word);
	n_word->freq = 1;
	n_word->next_word = NULL;
	n_word->array = (Array_SavePoints_T) tc_malloc(sizeof(struct Array_SavePoints));
	return n_word;
}

SavePoints_T AddSavePoints (char *word, char *filename, int line) {
	SavePoints_T sp = (SavePoints_T) tc_malloc(sizeof(struct SavePoints));
	sp->filename = (char *) tc_malloc(MAXCHAR);
	strcpy(sp->filename, filename);
	sp->line = line;
	sp->next_point = NULL;
	return sp;
}

int check_alphabet (char ch) {
	if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z')) {
		return 1;
	}
	else return 0;
}

int read_data(int fd, char *buffer, int buf_size){
	int size = 0;
	int len;

	while (1) {
		if ((len = read(fd, &buffer[size], buf_size-size)) > 0) {
			size += len;
			if (size == buf_size) { return size; }
		}
		else if (len == 0) { return size; }
		else { return -1; }
	}
}

DB_T bootstrapping (char* abs_path, DB_T n_DB) {

	DIR *dir_ptr = NULL;
	struct dirent *file = NULL;
	int cl, len = 0, fd = 0;
	char buffer[MAXCHAR] = {0}, temp_buffer[MAXCHAR] = {0};
	char n_word;
	int WORD_STATE = 0, WORD_LEN = 0;

	if ((dir_ptr = opendir(abs_path)) == NULL) {
		fprintf(stderr, "cannot open directory %s\n", abs_path);
	}

	while ((file = readdir(dir_ptr)) != NULL) {
		char *f_name = (char *)tc_malloc(MAXCHAR); //name of the file
		int line = 1;

		strcpy(f_name, abs_path);
		f_name = strcat(strcat(f_name, "/"), file->d_name);
		if (strcmp(file->d_name, ".") == 0) continue;
		if (strcmp(file->d_name, "..") == 0 ) continue;
		if (strcmp(file->d_name, ".DS_Store") == 0) continue;

		// open each file
		if ((fd = open(f_name, O_RDONLY)) == -1) {
			// error while opening the file
			fprintf (stderr, "error while opening the file: %s\n", file->d_name);
			printf("error code: %d\n", errno);
			continue;
		}
		// success opening the file -> read
		while (1) {
			int seek = 0;
			if ((len = read_data(fd, buffer, MAXCHAR)) == -1){
				fprintf(stderr, "error while reading the file: %s\n", file->d_name);
				break;
			}
			// success reading the file -> parse word and add to hash table
			while (seek < len) {
				n_word = buffer[seek];
				if (check_alphabet(n_word) == 0) { // NOT alphabet
					if (WORD_STATE == 1) {
						// make a word
						// check hash -> save or add
						//printf("New added word: %s\n", temp_buffer);
						CheckExistence(n_DB->ht_word, temp_buffer, file->d_name, line);
						//if (strcmp(file->d_name, "doc4.txt")==0) printf("WORD: %s\n", temp_buffer);
						WORD_STATE = 0;
					}
					if (n_word == '\n') line += 1;
				}
				else { // IS alphabet
					if (WORD_STATE == 0) {
						// start making a word
						memset(temp_buffer, 0, MAXCHAR);
						WORD_LEN = 0;
						WORD_STATE = 1;
					}
					temp_buffer[WORD_LEN] = n_word;
					WORD_LEN += 1;
				}
				seek += 1;
			}
			if (len == 0) { break; }
		}
		//close file
		if ((cl = close(fd)) == -1) {
			fprintf(stderr, "Error while closing the file\n");
			break;
		}
		if (WORD_STATE == 1) {
			CheckExistence(n_DB->ht_word, temp_buffer, file->d_name, line);
			WORD_STATE = 0;
			memset(temp_buffer, 0, MAXCHAR);
			WORD_LEN = 0;
		}
	}
	return n_DB;
}

/*--------------------------------------------------------------------*/

char* searching (HashTable_T ht, char* s_word){
	Word_T check = NULL;
	SavePoints_T s_point = NULL;
	char* return_buf = (char *)tc_malloc(5000); //////////////////change@@@
	//memcpy(return_buf, 0, 5000);
	char* prev_filename = (char *)tc_malloc(MAXCHAR);
	int prev_line = 0;
	int offset = 0;

	int h_num = hash_function(s_word, HASHLEN);
	for (check = ht->array[h_num]; check != NULL; check = check->next_word){
		if (strcmp(check->word, s_word) == 0){
			s_point = check->array->array[0];
			while (s_point != NULL) {
				if (strcmp(s_point->filename, prev_filename) || s_point->line!=prev_line){
					
					//printf("%s: line #%d\n", s_point->filename, s_point->line);
					char temp[MAXBUF] = "";
					sprintf(temp, "%s: line #%d\n", s_point->filename, s_point->line);
					strncpy(return_buf+offset, temp, strlen(temp)+1);
					offset += strlen(temp);
				}
				strcpy(prev_filename, s_point->filename);
				prev_line = s_point->line;
				s_point = s_point->next_point;
			}
			return return_buf;
		}
	}
	return NULL;
}

/*--------------------------------------------------------------------*/

int fd_read(int fd, void *dbuf, size_t n){
    ssize_t rd = 0;
    size_t tr = 0;
    do {
        rd = read(fd, dbuf+tr, n);
        //printf("in while while rd: %d\n", rd);
        if (rd < 0) return -1;
        if (rd == 0) break;
        tr += rd;
    }while (tr < n);
    return tr;
}

int workdone = 0;
void* inverted_index (void *input){
	char s_input[MAXBUF] = "";
	char s_buf[MAXBUF] = "";
	char buf[MAXBUF] = "";
	char* temp = "";
	int temp_num = 0; 
	uint32_t readnum = 0;
	char* result;
	int connfd = ((threadset_T) input)->fd;
	DB_T n_DB = ((threadset_T) input)->curDB;

	//tc_thread_init();

   //fd_read(connfd, buf, sizeof(buf));

	//read protocol//
	readnum = 0;
	while (readnum < 4) {
		temp_num = read(connfd, buf+readnum, sizeof(buf));
		readnum += temp_num;
	}
	int _total_length = ((protocol_T)buf)->total_length;

	while (readnum < 8){
		temp_num = read(connfd, buf+readnum, sizeof(buf));
		readnum += temp_num;
	}
	int _MSG_type = ((protocol_T)buf)->MSG_type;

	while (readnum < _total_length){
		temp_num = read(connfd, buf+readnum, sizeof(buf));
		readnum += temp_num;
	}

	//printf("total_length: %d\n", ((protocol_T)buf)->total_length);
	//printf("MSG_type: %d\n", ((protocol_T)buf)->MSG_type);
	//printf("payload: %s\n", buf+8);
	
	//search//
	result = searching(n_DB->ht_word, buf+8);

	if (result == NULL){
		printf("no such word exists\n");

		//structure for protocol

		//printf("locking: malloc for NULL\n");
		//pthread_spin_lock(&spinlock);
		protocol_T p_output = (protocol_T) tc_malloc(8);
		//pthread_spin_unlock(&spinlock);

		p_output->total_length = 8;
		p_output->MSG_type = 32;
		p_output->payload = NULL;
		write (connfd, (char*)p_output, 8);
		close(connfd);
		tc_free(p_output);
	}

	else {
		uint32_t payloadSize = strlen(result);
		
		//structure for protocol

		//printf("locking: malloc for element\n");
		//pthread_spin_lock(&spinlock);
		protocol_T p_output = (protocol_T) tc_malloc(payloadSize+1+8);
		//pthread_spin_unlock(&spinlock);

		p_output->total_length = payloadSize+1+8;
		p_output->MSG_type = 17;
		p_output->payload = (char *)p_output+8;
		//printf("returning length: %u\n", p_output->total_length);
		memcpy((char*)p_output+8, result, payloadSize+1);
		//printf("%lu) returning: %s\n", (unsigned long)gettid(), (char *)p_output+8);
		write(connfd, (char*)p_output, payloadSize+1+8);
		close(connfd);
		tc_free(p_output);

	}

   tc_free(input);

	return NULL;
}

int main(int argc, char **argv) {

	char* abs_dir = argv[1]; //path to find the folder
	int serverport = atoi(argv[2]);

	int rc, desc_ready, errno;
   int on = 1;
	int listen_fd, max_fd, new_fd, accept_fd;
	struct sockaddr_in addr;
	int end_server = 0;
 
	//tcmalloc
	spinlock_init();
	//printf("locking...\n");
	//pthread_spin_lock(&spinlock);
	tc_central_init();
	printf("central init\n");
	tc_thread_init();
	printf("thread init\n");
	//pthread_spin_unlock(&spinlock);
	
	DB_T n_DB = CreateWordDB();
	n_DB = bootstrapping(abs_dir, n_DB);

	fd_set master_set, working_set;
	
    struct timeval {
		long tv_sec;
		long tv_usec;
	};
	struct timeval timeout;


	/*
		Create an AF_INET6 stream socket to receive incoming
	*/

	listen_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_fd < 0) {
		perror("socket() failed");
		exit(-1);
	}

	/*
		Allow socket descriptor to be reuseable
	*/
	rc = setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on));
	if (rc < 0){
		perror("setsockopt() failed");
		close(listen_fd);
		exit(-1);
	}

	/*
		Set socket to be nonblocking. 
		All of the sockets for the incoming connections will also be nonblocking since they will inherit that state from the listening socket
	*/
	rc = ioctl(listen_fd, FIONBIO, (char *)&on);
	if (rc < 0){
		perror("ioctl() failed");
		close(listen_fd);
		exit(-1);
	}

	/*
		Bind the socket
	*/
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	//memcpy(&addr.sin_addr, &inaddr_any, sizeof(inaddr_any));
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(serverport);
	rc = bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr));
	if (rc < 0){
		perror("bind() failed");
		close(listen_fd);
		exit(-1);
	}

	/*
		Set the listen back log
	*/
	rc = listen(listen_fd, 100);
	if (rc < 0){
		perror("listen() failed");
		close(listen_fd);
		exit(-1);
	}

	/*
		Initialize the master fd_set
	*/
	FD_ZERO(&master_set); // initialize bitmask
	max_fd = listen_fd;
	FD_SET(listen_fd, &master_set); //set in bitmask

	//threadpool t_pool = thpool_init(10);
	threadpool_T threadpool = create_threadpool();
	//printf("Done Creating Pool\n");
	/*
		Initialize the timeval struct to 3 minutes. 
		If there are no activity after 3 minutes, this program will end
	*/
	//timeout.tv_sec = 3*60;
	//timeout.tv_usec = 0;

	/*
		Loop waiting for incoming connects or for incoming data
		or any of the connected sockets
	*/
	do {
      FD_ZERO(&working_set);
		/* copy the master fd set over to the working fd_set */
		memcpy(&working_set, &master_set, sizeof(master_set));

		/* call select() and wait */
		//printf("%lu) Waiting on select()...\n", (unsigned long)gettid());
		rc = select(max_fd+1, &working_set, NULL, NULL, NULL);

		if (rc<0) {
         printf("failed in select\n");
			perror("select() failed");
			break;
		}

		if (rc == 0) {
			printf("select() timed out. End program\n");
			break;
		}

		/* 
			One or more descriptors are readable. 
			Need to determine which ones they are
		*/
		//printf("%lu) NOTHING wrong with select output\n", (unsigned long)gettid());
		desc_ready = rc;
		for (int i=0; (i<=max_fd) && (desc_ready > 0); ++i){
			/* Check to see if this descriptor is ready */
			if (FD_ISSET(i, &working_set)) {
				desc_ready -= 1;

				/* Add the new incoming connection to the master read set */
				if (i == listen_fd) {
					int test = 0;
					while ((new_fd = accept(listen_fd, NULL, NULL))){
						if (new_fd < 0){
							if (errno != EWOULDBLOCK) {
								perror("accept() failed");
								end_server = TRUE;
							}
							break;
						}
				        FD_SET(new_fd, &master_set);
					    if (new_fd > max_fd) max_fd = new_fd;
                    }
					
				}

				else {
					struct threadset *input = tc_malloc(sizeof(struct threadset));
					input->fd = i;
					input->master_set_T = &master_set;
					input->working_set_T = &working_set;
					input->curDB = n_DB;
					//printf("tpool_add_work\n");
					add_work_threadpool(threadpool, (void *)inverted_index, (void *)input);
					//printf("Done tpool_add_work\n");
					FD_CLR(i, &master_set);
					//printf("done FD_CLR\n");
				}
			}
		}
	}while (end_server==FALSE);
	
	printf("out of do-while\n");
	return 1;
}


