
typedef struct t_worker* t_worker_T;
typedef struct threadpool* threadpool_T;
typedef void (*threadFunc)(void *arg);

threadpool_T create_threadpool();
bool add_work_threadpool(threadpool_T tp, threadFunc th_function, void *arg);

t_worker_T create_worker(threadFunc th_function, void* arg);
void destroy_worker(t_worker_T work);
t_worker_T get_first_worker(threadpool_T tp);
void *work_worker(void *arg);

