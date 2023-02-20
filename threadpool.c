#include <malloc.h>
#include "threadpool.h"

threadpool* create_threadpool(int num_threads_in_pool) {
    if (num_threads_in_pool <= 0 || num_threads_in_pool > MAXT_IN_POOL) {
        return NULL;
    }
    threadpool* pool_ = (threadpool*) malloc(sizeof(threadpool));
    if (pool_ == NULL) {
        return NULL;
    }

    //init pool vars
    pool_->num_threads = num_threads_in_pool;
    pool_->qsize = 0;
    pool_->qhead = NULL;
    pool_->qtail = NULL;
    pool_->shutdown = 0;
    pool_->dont_accept = 0;

    pool_->threads = (pthread_t*) malloc(num_threads_in_pool * sizeof(pthread_t));
    if (pool_->threads == NULL) {
        free(pool_);
        return NULL;
    }
    // init mutex & conditions
    pthread_mutex_init(&(pool_->qlock), NULL);
    pthread_cond_init(&(pool_->q_not_empty), NULL);
    pthread_cond_init(&(pool_->q_empty), NULL);

    // threads creating
    int i = 0;
    while (i < num_threads_in_pool) {
        if (pthread_create(&(pool_->threads[i]), NULL, do_work, (void*) pool_) != 0) {
            // error creating thread
            destroy_threadpool(pool_);
            return NULL;
        }
        i++;
    }
    return pool_;
}

void dispatch(threadpool* from_me, dispatch_fn dispatch_to_here, void *arg) {
    if(from_me == NULL  || dispatch_to_here == NULL || from_me->dont_accept) {
        return;
    }
    pthread_mutex_lock(&from_me->qlock);
    // create work_t element
    work_t* work = (work_t*) malloc(sizeof(work_t));
    while (from_me->dont_accept )
        pthread_cond_wait(&from_me->q_empty,&from_me->qlock);
    work->routine = dispatch_to_here;
    work->arg = arg;
    work->next = NULL;
    if(from_me == NULL || dispatch_to_here == NULL)
        return;
    if(from_me->dont_accept)
        return;

    // add work_t element to queue
    if (from_me->qsize == 0) {
        from_me->qhead = work;
        from_me->qtail = work;
        from_me->qsize++;
    } else {
        from_me->qtail->next = work;
        from_me->qtail = work;
        from_me->qsize++;
    }
    pthread_cond_signal(&(from_me->q_not_empty));
    pthread_mutex_unlock(&(from_me->qlock));
}

void* do_work(void* p){
    threadpool* pool_ = (threadpool*) p;
    while (1) {
        pthread_mutex_lock(&(pool_->qlock));
        // wait if queue is empty
        while (pool_->qsize == 0 && !pool_->shutdown) {
            pthread_cond_wait(&pool_->q_not_empty, &pool_->qlock);
        }
        // if shutdown is signaled and queue is empty, exit
        if (pool_->shutdown == 1) {
            pthread_mutex_unlock(&(pool_->qlock));
            pthread_exit(NULL);
        }

        // take first element from queue
        work_t* work = pool_->qhead;
        pool_->qsize--;
        if (pool_->qsize == 0) {
            pool_->qtail = NULL;
            pool_->qhead = NULL;
            pthread_cond_signal(&(pool_->q_empty));
        } else {
            pool_->qhead = pool_->qhead->next;
        }
        // unlock mutex
        pthread_mutex_unlock(&(pool_->qlock));
        // call thread routine
        (*(work->routine))(work->arg);
        // free the allocated memory for the work struct
        free(work);
        if (pool_->shutdown == 1 && pool_->qsize == 0) {
            pthread_exit(NULL);
        }
    }
}

void destroy_threadpool(threadpool* destroyme) {
    if(destroyme == NULL)
        return;

    pthread_mutex_lock(&(destroyme->qlock));
    // set don't accept flag and broadcast to threads
    destroyme->dont_accept = 1;
    pthread_cond_broadcast(&(destroyme->q_not_empty));

    // wait for all threads to exit
    int i = 0;
    while (i < destroyme->num_threads) {
        pthread_join(destroyme->threads[i], NULL);
        i++;
    }

    // free memory for threadPool and work_t elements
    work_t *work;
    while (destroyme->qsize > 0) {
        work = destroyme->qhead;
        destroyme->qhead = destroyme->qhead->next;
        free(work);
        destroyme->qsize--;
    }

    // destroy mutex and condition variables
    pthread_mutex_unlock(&(destroyme->qlock));
    pthread_mutex_destroy(&(destroyme->qlock));
    pthread_cond_destroy(&(destroyme->q_not_empty));
    pthread_cond_destroy(&(destroyme->q_empty));
    free(destroyme->threads);
    free(destroyme);
}