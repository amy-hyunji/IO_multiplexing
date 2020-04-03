
struct c_input {
	int thread_num;
	int request_num;
	struct sockaddr_in* server_addr;
};

struct protocol {
	uint32_t total_length;
	uint32_t MSG_type;
	char* payload;
};

typedef struct c_input* c_input_T;
typedef struct protocol* protocol_T;

int fd_read(int fd, void* dbuf, size_t n, int len);
void* connection(void *arg);
int main(int argc, char *argv[]);

