#include <assert.h>
#include <stdlib.h>
#include <ucontext.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include "thread.h"
#include "interrupt.h"

/* This is the thread control block */

//define constants
#define EXIT 2
#define READY 1
#define NONE_STATE -1

struct thread {
    /* ... Fill this in ... */
    int thread_id;
    int thread_state;
    int next_ready;
    ucontext_t thread_context;
    void * stack_ptr;
    bool stack_ptr_to_NULL;
};

//keep track of the last thread in ready queue
int ready_queue_tail_id = -1;
int ready_queue_head_id = -1;
int wait_queue_head_id = -1;
int wait_queue_tail_id = -1;
//keep track of current running thread
int current_thread = 0;
int count_create = 0;
//statically allocate the array to store threads
struct thread thread_array[THREAD_MAX_THREADS];

//statically allocate the exit array
int exit_array[THREAD_MAX_THREADS] = {-1};


//clean exit queue

void clean_exit_array() {
    //clean all exit state threads in exit queue
    int iter = 0;
    for (iter = 0; iter < THREAD_MAX_THREADS; iter++) {
        if (exit_array[iter] == -1)
            break;
        else {
            //free current thread
            thread_array[exit_array[iter]].next_ready = -1;
            free(thread_array[exit_array[iter]].stack_ptr);
            thread_array[exit_array[iter]].stack_ptr = NULL;
            thread_array[exit_array[iter]].stack_ptr_to_NULL = true;
            thread_array[exit_array[iter]].thread_state = NONE_STATE;
            exit_array[iter] = -1;
        }
    }
}

/*void print_wait(struct wait_queue * queue){
    //disables interrupt
    int enabled;
    enabled = interrupts_off();
    struct wait_queue * current = queue;
    while (current != NULL){
        printf("%d -> ", current ->thread_ID);
        current = current->next;
    }
    //restore interrupt state
    enabled = interrupts_set(1);
    assert(!enabled);
}*/

void print_threads() {
    int enabled;
    enabled = interrupts_off();
    //assert(enabled);

    printf("ready tail = %d, ready head = %d\n", ready_queue_tail_id, ready_queue_head_id);
    int current = ready_queue_head_id;
    while (thread_array[current].next_ready != -1) {
        if (thread_array[current].thread_state == EXIT)
            printf("haha\n");
        printf("thread %d -> ", current);
        current = thread_array[current].next_ready;
    }
    printf("thread %d -> ", current);

    //restore interrupt state
    enabled = interrupts_set(1);
    assert(!enabled);
}

//print head and tail of ready queue, exit queue elements

void print_head() {
    printf("\nready tail = %d, ready head = %d, current thread = %d\n", ready_queue_tail_id, ready_queue_head_id, current_thread);
    int iter = 0;
    for (iter = 0; iter < THREAD_MAX_THREADS; iter++) {
        if (exit_array[iter] == -1)
            break;
        else {
            printf("exit %d ", exit_array[iter]);
        }
    }
}
//stub function

/* thread starts by calling thread_stub. The arguments to thread_stub are the
 * thread_main() function, and one argument to the thread_main() function. */
void
thread_stub(void (*thread_main)(void *), void *arg) {
    Tid ret;
    clean_exit_array();

    //restore interrupt state
    interrupts_on();

    thread_main(arg); // call thread_main() function with arg
    ret = thread_exit();
    // we should get to the next line only if we are the last thread. 
    assert(ret == THREAD_NONE);
    // all threads are done, so process should exit
    exit(0);
}

void
thread_init(void) {
    /* your optional code here */
    //disables interrupt
    int enabled;
    enabled = interrupts_off();
    int iter = 0;
    for (iter = 0; iter < THREAD_MAX_THREADS; iter++) {
        thread_array[iter].thread_id = iter;
        thread_array[iter].next_ready = -1;
        thread_array[iter].thread_state = NONE_STATE;
        thread_array[iter].thread_context.uc_mcontext.gregs[REG_RSP] = (long long int) NULL;
        thread_array[iter].stack_ptr = NULL;
        thread_array[iter].stack_ptr_to_NULL = true;
    }
    //restore interrupt state
    enabled = interrupts_set(enabled);
}

Tid
thread_id() {
    int enabled;
    enabled = interrupts_off();

    int thread_index = thread_array[current_thread].thread_id;
    if (thread_index >= 0 && thread_index <= THREAD_MAX_THREADS) {
        interrupts_set(enabled);
        return thread_index;
    } else {
        interrupts_set(enabled);
        return THREAD_INVALID;
    }
}

Tid
thread_create(void (*fn) (void *), void *parg) { //do not understand the fn type
    //disables interrupt
    int enabled;
    enabled = interrupts_off();

    bool found_available = false;
    int iter = 0;
    for (iter = 1; iter < THREAD_MAX_THREADS; iter++) {
        if ((thread_array[iter].stack_ptr_to_NULL == true) | (thread_array[iter].thread_state == NONE_STATE)) {
            found_available = true;
            break;
        }
    }
    //iter holds the available thread index
    if (found_available == false) {
        interrupts_set(enabled);
        return THREAD_NOMORE;
    }

    //check if this is a new thread or an exited thread
    //if yes, resume the thraed into a new thread
    if (thread_array[iter].thread_state == EXIT) {
        free(thread_array[iter].stack_ptr);
        thread_array[iter].next_ready = -1;
        thread_array[iter].thread_state = NONE_STATE;
        thread_array[iter].stack_ptr = NULL;
        thread_array[iter].stack_ptr_to_NULL = true;
    }

    //update the current thread context for the first time
    int err = getcontext(&thread_array[iter].thread_context);
    assert(!err);
    //calculate address for stack pointer to ensure frame pointer is aligned
    void * mallPtr = malloc(THREAD_MIN_STACK + 24);
    //original stack top pointer
    thread_array[iter].stack_ptr = mallPtr;
    //check if memory is full
    if (mallPtr == NULL) {
        //restore interrupt state
        interrupts_set(enabled);
        return THREAD_NOMEMORY;
    }
    void * stackPtr = mallPtr + THREAD_MIN_STACK + 24;
    void * framPtr = stackPtr - 8;
    int offset = (uintptr_t) framPtr % 16; //?
    stackPtr = stackPtr - offset;

    //four changes
    //initialize stack pointer in context
    thread_array[iter].thread_context.uc_mcontext.gregs[REG_RSP] = (long long int) stackPtr;
    //point program counter to point to stub function that will run next
    thread_array[iter].thread_context.uc_mcontext.gregs[REG_RIP] = (long long int) thread_stub;
    //first argument of stub function is fn
    thread_array[iter].thread_context.uc_mcontext.gregs[REG_RDI] = (long long int) fn;
    //second argument of stub is prag with pointer type to anything (void *))
    thread_array[iter].thread_context.uc_mcontext.gregs[REG_RSI] = (long long int) parg;
    //ordinary changes in thread struct
    thread_array[iter].stack_ptr_to_NULL = false;
    thread_array[iter].thread_state = READY;

    //initial condition when tail and head are not initialized yet (=-1)
    if (ready_queue_tail_id == -1 && ready_queue_head_id == -1) {
        ready_queue_tail_id = iter;
        ready_queue_head_id = iter;
        thread_array[ready_queue_tail_id].next_ready = -1;
        //restore interrupt state
        interrupts_set(enabled);
        return iter;
    } else {
        //put the newly created thread in ready queue
        thread_array[ready_queue_tail_id].next_ready = iter;
        ready_queue_tail_id = iter;
        //restore interrupt state
        interrupts_set(enabled);
        return iter;
    }
    //restore interrupt state
    interrupts_set(enabled);
    return 0;
}

Tid
thread_yield(Tid want_tid) {
    //upon yield, update current thread context, switch to the want_id thread
    //determine if the caller thread is in exit state, if so put it in exit,
    //if not put caller at the end of ready queue
    //printf("ready tail in = %d, ready head in = %d\n", ready_queue_tail_id, ready_queue_head_id);
    //printf("want_id = %d\n", want_tid);
    //printf("current_thread = %d\n", current_thread);
    //print_threads();

    //disables interrupt
    int enabled;
    enabled = interrupts_off();
    //assert(enabled);

    if (want_tid == THREAD_SELF) {
        //no-op
        //restore interrupt state
        interrupts_set(enabled);
        //assert(!enabled);
        return current_thread;
    }
    if (want_tid == current_thread) {
        //no op
        //restore interrupt state
        interrupts_set(enabled);
        //assert(!enabled);
        return current_thread;
    }
    if ((want_tid > THREAD_MAX_THREADS) | (want_tid < -7)) {
        //restore interrupt state
        interrupts_set(enabled);
        //assert(!enabled);
        return THREAD_INVALID;
    }

    if (ready_queue_tail_id == -1 && ready_queue_head_id == -1) {
        if (want_tid == THREAD_ANY) {
            //restore interrupt state
            interrupts_set(enabled);
            //assert(!enabled);
            return THREAD_NONE;
        } else {
            //restore interrupt state
            interrupts_set(enabled);
            //assert(!enabled);
            return THREAD_INVALID;
        }
    }
    int thread_to_yield = -1;
    int caller_thread_id = current_thread;

    //exit
    if (thread_array[current_thread].thread_state == EXIT) {
        //current is exit state
        if (ready_queue_tail_id == ready_queue_head_id) {
            //there is only one thread in ready queue
            if (want_tid == ready_queue_tail_id) {
                //used the last thread in ready queue
                ready_queue_tail_id = -1;
                ready_queue_head_id = -1;
                thread_to_yield = want_tid;
                thread_array[thread_to_yield].next_ready = -1;
            } else if (want_tid == THREAD_ANY) {
                thread_to_yield = ready_queue_tail_id;
                ready_queue_tail_id = -1;
                ready_queue_head_id = -1;
                thread_array[thread_to_yield].next_ready = -1;
            } else {
                //restore interrupt state
                interrupts_set(enabled);
                //assert(!enabled);
                return THREAD_INVALID;
            }
        } else {
            //caller is in exit state, if process, do not put in ready queue
            //there are more than one threads in ready queue
            //check if yield can proceed
            if (want_tid == THREAD_ANY) {
                thread_to_yield = ready_queue_head_id;
                ready_queue_head_id = thread_array[ready_queue_head_id].next_ready;
                thread_array[thread_to_yield].next_ready = -1;
            } else {
                //go through the ready list try to find the wanted id
                //though an exit state thread only calls yield THREAD_ANY
                int temp_thread_id = ready_queue_head_id;
                int pre_thread_id = temp_thread_id;
                while (temp_thread_id != -1 && temp_thread_id != want_tid) {
                    pre_thread_id = temp_thread_id;
                    temp_thread_id = thread_array[temp_thread_id].next_ready;
                }
                if (temp_thread_id == -1) {
                    //restore interrupt state
                    interrupts_set(enabled);
                    //assert(!enabled);
                    return THREAD_INVALID;
                } else {
                    //temp_thread_id == want_tid
                    thread_array[pre_thread_id].next_ready = thread_array[want_tid].next_ready;
                    thread_to_yield = temp_thread_id;
                    if (want_tid == ready_queue_head_id)
                        ready_queue_head_id = thread_array[thread_to_yield].next_ready;
                    thread_array[thread_to_yield].next_ready = -1;
                    //check if the wanted thread is the end thread in ready
                    if (want_tid == ready_queue_tail_id)
                        ready_queue_tail_id = pre_thread_id;

                }
            }
        }
    }//not exit
    else if (ready_queue_tail_id == ready_queue_head_id) {
        //current thread is not exit state, put it back to ready queue
        //there is only one thread in ready queue
        if (want_tid == ready_queue_tail_id) {
            //used the last thread in ready queue
            thread_to_yield = want_tid;
            ready_queue_tail_id = caller_thread_id;
            ready_queue_head_id = caller_thread_id;
            thread_array[thread_to_yield].next_ready = -1;
            thread_array[caller_thread_id].next_ready = -1;
        } else if (want_tid == THREAD_ANY) {
            thread_to_yield = ready_queue_tail_id;
            ready_queue_tail_id = caller_thread_id;
            ready_queue_head_id = caller_thread_id;
            thread_array[thread_to_yield].next_ready = -1;
            thread_array[caller_thread_id].next_ready = -1;
        } else {
            //only one and it is not the right one
            //restore interrupt state
            interrupts_set(enabled);
            //assert(!enabled);
            return THREAD_INVALID;
        }
    } else {
        //current thread state is not exit state, put it back to ready queue
        //there are multiple threads in ready queue
        //check if yield can proceed
        if (want_tid == THREAD_ANY) {
            thread_to_yield = ready_queue_head_id;
            ready_queue_head_id = thread_array[ready_queue_head_id].next_ready;
            thread_array[ready_queue_tail_id].next_ready = caller_thread_id;
            ready_queue_tail_id = caller_thread_id;
            thread_array[ready_queue_tail_id].next_ready = -1;
            thread_array[thread_to_yield].next_ready = -1;
        } else {
            //go through the ready list try to find the wanted id
            int temp_thread_id = ready_queue_head_id;
            int pre_thread_id = temp_thread_id;
            while (temp_thread_id != -1 && temp_thread_id != want_tid) {
                pre_thread_id = temp_thread_id;
                temp_thread_id = thread_array[temp_thread_id].next_ready;
            }
            if (temp_thread_id == -1) {
                //restore interrupt state
                interrupts_set(enabled);
                //assert(!enabled);
                return THREAD_INVALID;
            } else {
                //temp_thread_id == want_tid
                thread_array[pre_thread_id].next_ready = thread_array[want_tid].next_ready;
                thread_to_yield = temp_thread_id;
                if (thread_to_yield != ready_queue_tail_id) {
                    thread_array[ready_queue_tail_id].next_ready = caller_thread_id;
                    ready_queue_tail_id = caller_thread_id;
                    if (thread_to_yield == ready_queue_head_id) {
                        ready_queue_head_id = thread_array[ready_queue_head_id].next_ready;
                    }
                    thread_array[thread_to_yield].next_ready = -1;
                    thread_array[ready_queue_tail_id].next_ready = -1;
                } else {
                    //thread_to_yield == ready_queue_tail_id
                    thread_array[pre_thread_id].next_ready = caller_thread_id;
                    ready_queue_tail_id = caller_thread_id;
                    thread_array[thread_to_yield].next_ready = -1;
                    thread_array[ready_queue_tail_id].next_ready = -1;
                }
            }
        }
    }

    if (thread_to_yield == -1) {
        //restore interrupt state
        interrupts_set(enabled);
        //assert(!enabled);
        return THREAD_INVALID;
    }

    //now thread_to_yield has get next valid switch thread

    //put the exit state previous thread into exit queue
    if (thread_array[current_thread].thread_state == EXIT) {
        //put this thread at the end of the exit array
        int iter = 0;
        for (iter = 0; iter < THREAD_MAX_THREADS; iter++) {
            if (exit_array[iter] == -1) {
                exit_array[iter] = current_thread;
                break;
            }
        }
    }

    //if the this call to yield comes from thread_sleep, then current thread is
    //already in wait queue. But up till now, the current thread has already
    //being put in the tail of ready queue. A thread cannot exit in both wait
    //queue and ready queue. Delete the current thread from the tail of ready
    //queue. Thread_to_yield has the yielding thread with next_ready = -1;
    //    if (current_thread == ready_queue_tail_id) {
    //        if (0) {
    //            //last element
    //            thread_array[ready_queue_head_id].next_ready = -1;
    //            ready_queue_head_id = -1;
    //            ready_queue_tail_id = -1;
    //        } else {
    //            int pre_a = ready_queue_head_id;
    //            int current_a = ready_queue_head_id;
    //            //printf("duplica = %d, head = %d, tail = %d\n", current_thread,ready_queue_head_id, ready_queue_tail_id);
    //
    //            /*while (thread_array[thread_array[pre_ready_queue_of_ready_tail].next_ready].thread_id != ready_queue_tail_id)
    //                pre_ready_queue_of_ready_tail = thread_array[pre_ready_queue_of_ready_tail].next_ready;*/
    //            //print_threads();
    //            while (current_a != ready_queue_tail_id) {
    //                pre_a = current_a;
    //                current_a = thread_array[current_a].next_ready;
    //            }
    //            ready_queue_tail_id = pre_a;
    //            thread_array[pre_a].next_ready = -1;
    //            thread_array[current_a].next_ready = -1;
    //        }
    //    }

    //context switch
    int first_come_flag = -1;
    int err_get = getcontext(&thread_array[caller_thread_id].thread_context);
    assert(!err_get);
    first_come_flag = first_come_flag * (-1);
    if (first_come_flag == 1) {
        //constant set up
        current_thread = thread_to_yield;
        int err_set = setcontext(&thread_array[current_thread].thread_context);
        assert(!err_set);
    }
    clean_exit_array();

    //restore interrupt state
    interrupts_set(enabled);
    //assert(!enabled);
    return thread_to_yield;
}

Tid
thread_exit() {
    //disables interrupt
    int enabled;
    enabled = interrupts_off();

    if ((ready_queue_head_id != -1) | (ready_queue_tail_id != -1)) {
        //there are threads or one thread in ready queue
        thread_array[current_thread].thread_state = EXIT;
        Tid next_thread = thread_yield(THREAD_ANY);
        assert(next_thread == next_thread);
        //return next_thread;
    }
    if ((ready_queue_head_id == -1) | (ready_queue_tail_id == -1)) {
        //there is no thread
        //restore interrupt state
        interrupts_set(enabled);
        return THREAD_NONE;
    }
    //restore interrupt state
    interrupts_set(enabled);
    return 0;
}

Tid
thread_kill(Tid tid) {
    //disables interrupt
    int enabled;
    enabled = interrupts_off();

    //check if this thread is valid
    if ((tid > THREAD_MAX_THREADS) | (tid < -7)) {
        //restore interrupt state
        interrupts_set(enabled);
        return THREAD_INVALID;
    }

    if ((tid == THREAD_SELF) | (tid == current_thread)) {
        //restore interrupt state
        interrupts_set(enabled);
        return THREAD_INVALID;
    }
    if (thread_array[tid].thread_state == NONE_STATE) {
        //restore interrupt state
        interrupts_set(enabled);
        return THREAD_INVALID;
    }
    if (thread_array[tid].thread_state == EXIT) {
        //no op 
        //there wont be any thread with EXIT state and another thread
        //is running, thread can only be in EXIT if it finishes thread_main
        //execution and calls thread_exit by itself, but in this case, it is
        //killed by the next yielding thread immediately
    }
    if (thread_array[tid].thread_state == READY) {
        //this thread must be in READY QUEUE
        //remove this thread from ready array
        int thread_to_kill = -1;
        if (ready_queue_tail_id == ready_queue_head_id) {
            //there is only one thread in ready queue
            if (tid == ready_queue_tail_id) {
                //used the last thread in ready queue
                ready_queue_tail_id = -1;
                ready_queue_head_id = -1;
                thread_to_kill = tid;
                thread_array[thread_to_kill].next_ready = -1;
            } else {
                //restore interrupt state
                interrupts_set(enabled);
                return THREAD_INVALID;
            }
        } else {
            //there are multiple threads available in ready array
            int temp_thread_id = ready_queue_head_id;
            int pre_thread_id = temp_thread_id;
            while (temp_thread_id != -1 && temp_thread_id != tid) {
                pre_thread_id = temp_thread_id;
                temp_thread_id = thread_array[temp_thread_id].next_ready;
            }
            if (temp_thread_id == -1) {
                //restore interrupt state
                interrupts_set(enabled);
                return THREAD_INVALID;
            } else {
                //temp_thread_id == tid
                thread_array[pre_thread_id].next_ready = thread_array[tid].next_ready;
                thread_to_kill = tid;

                //check if the wanted thread is the end thread in ready
                if (tid == ready_queue_tail_id)
                    ready_queue_tail_id = pre_thread_id;
                if (tid == ready_queue_head_id)
                    ready_queue_head_id = thread_array[tid].next_ready;
                thread_array[tid].next_ready = -1;
            }
        }
        if (thread_to_kill == -1) {
            //restore interrupt state
            interrupts_set(enabled);
            return THREAD_INVALID;
        }
        //kill thread_to_kill
        thread_array[thread_to_kill].next_ready = -1;
        free(thread_array[thread_to_kill].stack_ptr);
        thread_array[thread_to_kill].stack_ptr = NULL;
        thread_array[thread_to_kill].stack_ptr_to_NULL = true;
        thread_array[thread_to_kill].thread_state = NONE_STATE;
        //restore interrupt state
        interrupts_set(enabled);
        return thread_to_kill;
    }
    //restore interrupt state
    interrupts_set(enabled);
    return 0;
}

/*******************************************************************
 * Important: The rest of the code should be implemented in Lab 3. *
 *******************************************************************/

/* This is the wait queue structure */
struct element {
    int next_thread_id;
};

struct wait_queue {
    int wait_queue_head;
    int wait_queue_tail;
    struct element Wait_queue[THREAD_MAX_THREADS];
};

struct wait_queue *
wait_queue_create() {
    //disables interrupt
    int enabled;
    enabled = interrupts_off();

    struct wait_queue *wq;

    //allocate the head node of the linked list
    wq = malloc(sizeof (struct wait_queue));
    assert(wq);

    wq->wait_queue_head = -1;
    wq->wait_queue_tail = -1;

    //restore interrupt state
    interrupts_set(enabled);
    return wq;
}

void
wait_queue_destroy(struct wait_queue * wq) {
    //disables interrupt
    int enabled;
    enabled = interrupts_off();

    free(wq);

    //restore interrupt state
    interrupts_set(enabled);
}

Tid
thread_sleep(struct wait_queue * queue) {
    //disables interrupt
    int enabled;
    enabled = interrupts_off();

    if (queue == NULL) {
        //restore interrupt state
        interrupts_set(enabled);
        return THREAD_INVALID;
    }
    if (ready_queue_tail_id == -1 && ready_queue_head_id == -1) {
        //there is no thread available
        //restore interrupt state
        interrupts_set(enabled);
        return THREAD_NONE;
    }

    //then there must be thread that can be yielded
    //insert current thread at the end of wait queue
    if (queue->wait_queue_head == -1 && queue->wait_queue_tail == -1) {
        queue->wait_queue_head = current_thread;
        queue->wait_queue_tail = current_thread;
    } else {
        queue->Wait_queue[queue->wait_queue_tail].next_thread_id = current_thread;
        queue->wait_queue_tail = current_thread;
    }
    int thread_to_yield = -1;
    if (ready_queue_tail_id == ready_queue_head_id) {
        //this is the last element
        //after this call, ready queue tail and head will be set to -1
        thread_to_yield = ready_queue_tail_id;
        thread_array[thread_to_yield].next_ready = -1;
        ready_queue_tail_id = -1;
        ready_queue_head_id = -1;
    } else {
        //there are more than one threads in the ready queue
        thread_to_yield = ready_queue_head_id;
        ready_queue_head_id = thread_array[ready_queue_head_id].next_ready;
        thread_array[thread_to_yield].next_ready = -1;
    }

    //thread_to_yield is yielding thread id
    //context switch
    int first_come_flag = -1;
    int err_get = getcontext(&thread_array[current_thread].thread_context);
    assert(!err_get);
    first_come_flag = first_come_flag * (-1);
    if (first_come_flag == 1) {
        //constant set up
        current_thread = thread_to_yield;
        int err_set = setcontext(&thread_array[current_thread].thread_context);
        assert(!err_set);
    }
    clean_exit_array();

    //restore interrupt state
    interrupts_set(enabled);
    //assert(!enabled);

    return thread_to_yield;
}

/* when the 'all' parameter is 1, wakeup all threads waiting in the queue.
 * returns whether a thread was woken up on not. */
int
thread_wakeup(struct wait_queue *queue, int all) {
    //disables interrupt
    int enabled;
    enabled = interrupts_off();

    //queue->next == NULL check for there is only one thread in wait queue
    //the empty initial one
    if (queue == NULL) {
        //restore interrupt state
        interrupts_set(enabled);
        return 0;
    } else if (queue->wait_queue_head == -1 && queue->wait_queue_tail == -1) {
        //wait queue is empty
        //restore interrupt state
        interrupts_set(enabled);
        return 0;
    }

    //up till now there is at least 2 threads in wait queue including the intial
    //empty one
    int thread_to_wake_up = -1;
    int count = 0;
    if (all == 0) {
        //wake up one thread
        thread_to_wake_up = queue->wait_queue_head;
        queue->wait_queue_head = queue->Wait_queue[queue->wait_queue_head].next_thread_id;
        //put current thread into ready queue tail
        if (ready_queue_tail_id == -1 && ready_queue_head_id == -1) {
            //no element in ready queue
            ready_queue_tail_id = thread_to_wake_up;
            ready_queue_head_id = thread_to_wake_up;
            thread_array[thread_to_wake_up].next_ready = -1;

            //restore interrupt state
            interrupts_set(enabled);
            return 1;
        } else {
            //normal case
            thread_array[ready_queue_tail_id].next_ready = thread_to_wake_up;
            thread_array[thread_to_wake_up].next_ready = -1;
            ready_queue_tail_id = thread_to_wake_up;

            //restore interrupt state
            interrupts_set(enabled);
            return 1;
        }
    } else if (all == 1) {
        //wake up all threads
        count = 0;
        while (queue->wait_queue_head != queue->wait_queue_tail) {
            thread_to_wake_up = queue ->wait_queue_head;
            queue->wait_queue_head = queue->Wait_queue[queue->wait_queue_head].next_thread_id;
            //put current thread into ready queue tail
            if (ready_queue_tail_id == -1 && ready_queue_head_id == -1) {
                //no element in ready queue
                ready_queue_tail_id = thread_to_wake_up;
                ready_queue_head_id = thread_to_wake_up;
                thread_array[thread_to_wake_up].next_ready = -1;
                count++;
            } else {
                //normal case
                thread_array[ready_queue_tail_id].next_ready = thread_to_wake_up;
                thread_array[thread_to_wake_up].next_ready = -1;
                ready_queue_tail_id = thread_to_wake_up;
                count++;
            }
        }
        //at here there is still one thread not being waken up
        //thread_to_wake_up = queue->wait_queue_head = queue->wait_queue_tail
        thread_to_wake_up = queue->wait_queue_head;
        if (ready_queue_tail_id == -1 && ready_queue_head_id == -1) {
            //no element in ready queue
            ready_queue_tail_id = thread_to_wake_up;
            ready_queue_head_id = thread_to_wake_up;
            thread_array[thread_to_wake_up].next_ready = -1;
            queue->wait_queue_head = -1;
            queue->wait_queue_tail = -1;
            count++;
        } else {
            //normal case
            thread_array[ready_queue_tail_id].next_ready = thread_to_wake_up;
            thread_array[thread_to_wake_up].next_ready = -1;
            ready_queue_tail_id = thread_to_wake_up;
            queue->wait_queue_head = -1;
            queue->wait_queue_tail = -1;
            count++;
        }
        //restore interrupt state
        interrupts_set(enabled);
        return count;
    } else {
        //input is illegal
        //restore interrupt state
        interrupts_set(enabled);
        return 0;
    }
    interrupts_set(enabled);
    return 0;
}

struct lock {
    /* ... Fill this in ... */
    int acquired_thread_id;
    struct wait_queue lock_wait_queue;
    int l;
};

struct lock *
lock_create() {
    //disables interrupt
    int enabled;
    enabled = interrupts_off();

    struct lock *lock;

    lock = malloc(sizeof (struct lock));
    assert(lock);
    lock->acquired_thread_id = -1;
    lock->l = 0;

    //restore interrupt state
    interrupts_set(enabled);
    return lock;
}

void
lock_destroy(struct lock * lock) {
    //disables interrupt
    int enabled;
    enabled = interrupts_off();

    assert(lock != NULL);

    if (lock->l == 0)

        free(lock);

    //restore interrupt state
    interrupts_set(enabled);
}

void
lock_acquire(struct lock * lock) {
    //disables interrupt
    int enabled;
    enabled = interrupts_off();

    assert(lock != NULL);

    while (lock->l == 1)
        thread_sleep(&(lock->lock_wait_queue));
    lock->l = 1;
    lock->acquired_thread_id = current_thread;

    //restore interrupt state
    interrupts_set(enabled);
}

void
lock_release(struct lock * lock) {
    //disables interrupt
    int enabled;
    enabled = interrupts_off();
    //assert(enabled);

    assert(lock != NULL);

    //if ((current_thread == lock->acquired_thread_id) && (lock->l == 1)) {
    if (lock->l == 1) {
        thread_wakeup(&(lock->lock_wait_queue), 1);
        //release the lock
        lock->l = 0;
        lock->acquired_thread_id = -1;
    }
    //restore interrupt state
    interrupts_set(enabled);
    //assert(!enabled);
}

struct cv {
    /* ... Fill this in ... */
    struct wait_queue cv_wait_queue;
};

struct cv *
cv_create() {
    int enabled;
    enabled = interrupts_off();

    struct cv *cv;

    cv = malloc(sizeof (struct cv));
    assert(cv);

    interrupts_set(enabled);
    return cv;
}

void
cv_destroy(struct cv * cv) {
    int enabled;
    enabled = interrupts_off();

    assert(cv != NULL);

    if ((cv->cv_wait_queue.wait_queue_head == -1) && (cv->cv_wait_queue.wait_queue_tail == -1)) {
        //wait queue is empty
        //no thread is waiting on this cv
        free(cv);
    }

    interrupts_set(enabled);
}

void
cv_wait(struct cv *cv, struct lock * lock) {
    int enabled;
    enabled = interrupts_off();

    assert(cv != NULL);
    assert(lock != NULL);

    if (lock->l == 1) {
        //release the lock
        lock_release(lock);
        thread_sleep(&(cv->cv_wait_queue));
        //reacquire the lock
        lock_acquire(lock);
    }

    interrupts_set(enabled);
}

void
cv_signal(struct cv *cv, struct lock * lock) {
    int enabled;
    enabled = interrupts_off();

    assert(cv != NULL);
    assert(lock != NULL);

    if (lock->l == 1) {
        thread_wakeup(&(cv->cv_wait_queue), 0);
    }

    interrupts_set(enabled);
}

void
cv_broadcast(struct cv *cv, struct lock * lock) {
    int enabled;
    enabled = interrupts_off();

    assert(cv != NULL);
    assert(lock != NULL);

    if (lock->l == 1) {
        thread_wakeup(&(cv->cv_wait_queue), 1);
    }

    interrupts_set(enabled);
}
