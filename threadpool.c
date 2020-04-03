#include <stdlib.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>

#include "threadpool.h"
#include "tc_malloc.h"

struct t_worker {
    threadFunc th_function;  
    void *arg;   
    t_worker_T next;  
};

struct threadpool {
    int workNum;  
    int threadNum;
	 t_worker_T first_work;   
    t_worker_T last_work;   
    pthread_mutex_t mutexLock;   
    pthread_cond_t is_work;    
    pthread_cond_t no_work; 
};

//create threadpool
threadpool_T create_threadpool(){
    pthread_t _thread;

    threadpool_T temp = calloc(1, sizeof(struct threadpool));
    temp->threadNum = 10;
	 temp->first_work = NULL;
    temp->last_work = NULL;
    pthread_mutex_init(&(temp->mutexLock), NULL);
    pthread_cond_init(&(temp->is_work), NULL);
    pthread_cond_init(&(temp->no_work), NULL);

    for (int i=0; i<10; i++) {
        pthread_create(&_thread, NULL, work_worker, temp);
        pthread_detach(_thread);
    }

    return temp;
}

//add on threadpool
bool add_work_threadpool(threadpool_T input, threadFunc th_function, void *arg){

    if (input == NULL) return false;

	 else {
		 t_worker_T temp = create_worker(th_function, arg);
		 if (temp == NULL) return false;
		 else {
			pthread_mutex_lock(&(input->mutexLock));
			if (input->first_work == NULL) {
				input->first_work = temp;
				input->last_work = input->first_work;
			}
			else {
				input->last_work->next = temp;
				input->last_work = temp;
			}
			pthread_cond_broadcast(&(input->is_work));
			pthread_mutex_unlock(&(input->mutexLock));
			return true;
		 }
	 }
}

t_worker_T create_worker(threadFunc th_function, void *arg){
    if (th_function == NULL) return NULL;
	 else {
		t_worker_T temp = malloc(sizeof(struct t_worker));
		temp->th_function = th_function;
		temp->arg = arg;
		temp->next = NULL;
		return temp;
	 }
}

void destroy_worker(t_worker_T work){
    if (work == NULL) return;
    else {
		 free(work);
		 return;
	}
}

t_worker_T get_first_worker(threadpool_T input){
    if (input == NULL) return NULL;

    else {
		 t_worker_T temp = input->first_work;
		 if (temp == NULL) return NULL;
		 else if (temp->next == NULL) {
			  input->first_work = NULL;
			  input->last_work  = NULL;
		 } 
		 else {
			  input->first_work = temp->next;
		 }
		 return temp;
	}
}

void *work_worker(void *arg){
    threadpool_T temp = arg;
    t_worker_T work;

	tc_thread_init();

    while (1) {

		  //lock
        pthread_mutex_lock(&(temp->mutexLock));

        if (temp->first_work == NULL)
            pthread_cond_wait(&(temp->is_work), &(temp->mutexLock));

        work = get_first_worker(temp);
        temp->workNum++;
        pthread_mutex_unlock(&(temp->mutexLock));
		  //unlock

        if (work != NULL) {
            work->th_function(work->arg);
            destroy_worker(work);
        }

		  //lock
        pthread_mutex_lock(&(temp->mutexLock));
        temp->workNum--;
        
        if ((temp->workNum == 0) && (temp->first_work == NULL))
            pthread_cond_signal(&(temp->no_work));
        pthread_mutex_unlock(&(temp->mutexLock));
		  //unlock
    }

    temp->threadNum--;
    if (temp->threadNum == 0)
        pthread_cond_signal(&(temp->no_work));
    pthread_mutex_unlock(&(temp->mutexLock));
    return NULL;
}


