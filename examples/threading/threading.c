#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{

    // TODO: wait, obtain mutex, wait, release mutex as described by thread_data structure
    // hint: use a cast like the one below to obtain thread arguments from your parameter
    //struct thread_data* thread_func_args = (struct thread_data *) thread_param;
    
    // Cast function param to thread_data
    struct thread_data* thread_data_ptr = (struct thread_data*)thread_param;
    // Sleep 
    usleep(thread_data_ptr->wait_to_obtain_ms);

    // Attempt to obtain mutex
    if (pthread_mutex_lock(thread_data_ptr->mutex) != 0) {
        perror("Could not obtain mutex");
        thread_data_ptr->thread_complete_success = false;
        return thread_data_ptr;
    }

    // Sleep
    usleep(thread_data_ptr->wait_to_release_ms);

    // Attempt to unlock mutext
    if (pthread_mutex_unlock(thread_data_ptr->mutex) != 0) {
        perror("Could not unlock mutex");
        thread_data_ptr->thread_complete_success = false;
        return thread_data_ptr;
    }

    thread_data_ptr->thread_complete_success = true;
    
    return thread_data_ptr;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
    /**
     * TODO: allocate memory for thread_data, setup mutex and wait arguments, pass thread_data to created thread
     * using threadfunc() as entry point.
     *
     * return true if successful.
     *
     * See implementation details in threading.h file comment block
     */

    struct thread_data* thread_data_ptr = (struct thread_data*)malloc(sizeof(struct thread_data));

    thread_data_ptr->mutex = mutex;
    thread_data_ptr->wait_to_obtain_ms = wait_to_obtain_ms;
    thread_data_ptr->wait_to_release_ms = wait_to_release_ms;
    thread_data_ptr->thread_complete_success = false;

    if (pthread_create(thread, NULL, threadfunc, (void*)thread_data_ptr) != 0) {
        perror("Could not create thread");
        free(thread_data_ptr);
        return false;
    }
    return true;
}
