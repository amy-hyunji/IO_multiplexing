typedef struct DB* DB_T;

pid_t gettid();

int spinlock_init();

struct threadset {
	int fd;
	fd_set* master_set_T;
	fd_set* working_set_T;
	DB_T curDB;
};

struct protocol {
	uint32_t total_length;
	uint32_t MSG_type;
	char* payload;
};

struct s_protocol {
	uint32_t total_length;
};

typedef struct s_protocol* s_protocol_T;
typedef struct threadset* threadset_T;
typedef struct HashTable* HashTable_T;
typedef struct SavePoints* SavePoints_T;
typedef struct Word* Word_T;
typedef struct Array_SavePoints* Array_SavePoints_T;
typedef struct protocol* protocol_T;

DB_T CreateWordDB(void);
HashTable_T CreateHashTable(void);
static int hash_function(const char* word, int iBucketCount);
int CheckExistence (HashTable_T ht, char* word, char* filename, int line);
Word_T AddNewWord(char* add_word);
SavePoints_T AddSavePoints(char *word, char* filename, int line);
int check_alphabet (char ch);
int read_data(int fd, char* buffer, int buf_size);
DB_T bootstrapping (char* abs_path, DB_T n_DB);
char* searching (HashTable_T ht, char* s_word);
int main(int argc, char** argv);
void* inverted_index (void *input);
