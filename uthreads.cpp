#include "uthreads.h"
#include <iostream>
#include <queue>
#include <setjmp.h>
#include <unistd.h>
#include <signal.h>
#include <new>
#include <stdio.h>
#include <sys/time.h>
#include <stdbool.h>


#ifdef __x86_64__
/* code for 64 bit Intel arch */

typedef unsigned long address_t;
#define JB_SP 6
#define JB_PC 7

/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr)
{
    address_t ret;
    asm volatile("xor    %%fs:0x30,%0\n"
        "rol    $0x11,%0\n"
                 : "=g" (ret)
                 : "0" (addr));
    return ret;
}

#else
/* code for 32 bit Intel arch */

typedef unsigned int address_t;
#define JB_SP 4
#define JB_PC 5


/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr)
{
    address_t ret;
    asm volatile("xor    %%gs:0x18,%0\n"
                 "rol    $0x9,%0\n"
    : "=g" (ret)
    : "0" (addr));
    return ret;
}


#endif


#define MAX_THREAD_NUM 100
#define STACK_SIZE 4096

using namespace std;

enum State {
    RUNNING,
    READY,
    BLOCKED
};

struct thread{
    State state;
    int quantum;
    char* stack;
    void (*func)(void);
    sigjmp_buf env;
    bool is_actively_blocked = false;
    int quantum_sleep = 0;
};

thread* threads[MAX_THREAD_NUM] = {nullptr};
queue<int> ready_queue;
int current_thread = -1;
int quantum_counter = 0;
int g_quantom_usecs = 0;


// blocks a timer signal (SIGVTALRM signal)
void block_timer_signal() {
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGVTALRM);

    if (sigprocmask(SIG_BLOCK, &set, NULL) < 0) {

        std::cerr << "system error: sigprocmask failed" << std::endl;

        exit(1);

    }
}


// unblocks the SIGVTALRM signal.
void unblock_timer_signal() {
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGVTALRM);
    if (sigprocmask(SIG_UNBLOCK, &set, NULL) < 0) {
        std::cerr << "system error: sigprocmask failed" << std::endl;
        exit(1);
    }
}

void reset_timer(int quantum_usecs) {
    struct itimerval timer;
    timer.it_value.tv_sec = quantum_usecs / 1000000;
    timer.it_value.tv_usec = quantum_usecs % 1000000;

    timer.it_interval.tv_sec = quantum_usecs / 1000000;
    timer.it_interval.tv_usec = quantum_usecs % 1000000;

    // Start a virtual timer. It counts down whenever this process is executing.
    if (setitimer(ITIMER_VIRTUAL, &timer, NULL))
    {
        std::cerr << "system error: " << "timer set error" << std::endl;
        exit(1);
    }
}


int context_switch() {
    block_timer_signal();

    // save current state
    if (threads[current_thread] != nullptr) {
        unblock_timer_signal();
        int result = sigsetjmp(threads[current_thread]->env,1);
        block_timer_signal();
        if (result != 0) {
            return 0;
        }
    }

    if (ready_queue.empty()) {
        return -1;
    }

    // update quantom-sleep for blocked threads
    for (int i = 0; i < MAX_THREAD_NUM; i++)  {
        if (threads[i] != nullptr && threads[i]->quantum_sleep > 0) {
            threads[i]->quantum_sleep--;
            if (threads[i]->quantum_sleep == 0 && threads[i]->is_actively_blocked == false) {
                ready_queue.push(i);
                threads[i]->state = READY;
            }
        }

    }

    // search for ready thread
    int next_thread = ready_queue.front();
    while (threads[next_thread] == nullptr || threads[next_thread]->state != READY) {
        ready_queue.pop();
        if (ready_queue.empty()) {
            return -1;
        }
        next_thread = ready_queue.front();
    }

    // before switch, reset the timer
    reset_timer(g_quantom_usecs);

    ready_queue.pop();
    current_thread = next_thread;
    threads[current_thread]->state = RUNNING;
    threads[current_thread]->quantum++;
    quantum_counter++;
    unblock_timer_signal();
    siglongjmp(threads[current_thread]->env,1);
}

void timer_handler(int sig)
{
    threads[current_thread]->state = READY;
    ready_queue.push(current_thread);
    context_switch();
}




/**
 * @brief initializes the thread library.
 *
 * Once this function returns, the main thread (tid == 0) will be set as RUNNING. There is no need to 
 * provide an entry_point or to create a stack for the main thread - it will be using the "regular" stack and PC.
 * You may assume that this function is called before any other thread library function, and that it is called
 * exactly once.
 * The input to the function is the length of a quantum in micro-seconds.
 * It is an error to call this function with non-positive quantum_usecs.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_init(int quantum_usecs) {

    block_timer_signal();

    if (quantum_usecs < 1) {
        std::cerr << "thread library error: " << "quantum must be positive integer" << std::endl;
        unblock_timer_signal();
        return -1;
    }
    g_quantom_usecs = quantum_usecs;
    thread* main = new (nothrow) thread{RUNNING, 1, nullptr};
    if (main == nullptr) {
        std::cerr << "system error: " << "allocation failed" << std::endl;
        exit(1);
    }
    threads[0] = main;
    current_thread = 0;
    quantum_counter++;


    struct sigaction sa = {};

    // Install timer_handler as the signal handler for SIGVTALRM.
    sa.sa_handler = &timer_handler;
    if (sigaction(SIGVTALRM, &sa, NULL) < 0)
    {
        std::cerr << "system error: " << "timer set error" << std::endl;
        exit(1);
    }

    reset_timer(quantum_usecs);

    unblock_timer_signal();

    return 0;

}

/**
 * @brief Creates a new thread, whose entry point is the function entry_point with the signature
 * void entry_point(void).
 *
 * The thread is added to the end of the READY threads list.
 * The uthread_spawn function should fail if it would cause the number of concurrent threads to exceed the
 * limit (MAX_THREAD_NUM).
 * Each thread should be allocated with a stack of size STACK_SIZE bytes.
 * It is an error to call this function with a null entry_point.
 *
 * @return On success, return the ID of the created thread. On failure, return -1.
*/
int uthread_spawn(thread_entry_point entry_point) {

    block_timer_signal();

    if (entry_point == nullptr) {
        std::cerr << "thread library error: " << "entry_point can't be NULL" << std::endl;
        unblock_timer_signal();
        return -1;
    }
    for (int i = 0; i < MAX_THREAD_NUM; i++) {
        if (threads[i] == nullptr) {

            // allocate memory for stack,thread
            char* stack = new (nothrow) char[STACK_SIZE];
            if (stack == nullptr) {
                std::cerr << "system error: " << "allocation failed" << std::endl;
                exit(1);
            }
            threads[i] = new (nothrow) thread{READY, 0, stack,entry_point};
            if (threads[i] == nullptr) {
                std::cerr << "system error: " << "allocation failed" << std::endl;
                exit(1);
            }

            // sigsetjmp also saves timer block
            unblock_timer_signal();
            sigsetjmp(threads[i]->env, 1);
            block_timer_signal();

            address_t sp = (address_t)stack + STACK_SIZE - sizeof(address_t);
            address_t pc = (address_t)entry_point;
            (threads[i]->env->__jmpbuf)[JB_SP] = translate_address(sp);
            (threads[i]->env->__jmpbuf)[JB_PC] = translate_address(pc);
            ready_queue.push(i);
            unblock_timer_signal();
            return i;
        }
    }
    // std::cerr << "thread library error: " << "Over 100 threads" << std::endl;
    unblock_timer_signal();
    return -1;
}


/**
 * @brief Terminates the thread with ID tid and deletes it from all relevant control structures.
 *
 * All the resources allocated by the library for this thread should be released. If no thread with ID tid exists it
 * is considered an error. Terminating the main thread (tid == 0) will result in the termination of the entire
 * process using exit(0) (after releasing the assigned library memory).
 *
 * @return The function returns 0 if the thread was successfully terminated and -1 otherwise. If a thread terminates
 * itself or the main thread is terminated, the function does not return.
*/
int uthread_terminate(int tid){

    block_timer_signal();

    if (tid == 0) {
        for (auto& thread : threads) {
            if (thread != nullptr) {
                delete[] thread->stack;
                delete thread;
            }
        }
        exit(0);
    }
    if (tid < 0 || tid >= MAX_THREAD_NUM) {
        std::cerr << "thread library error: " << "tid must be positive" << std::endl;
        unblock_timer_signal();
        return -1;
    }
    if (threads[tid] == nullptr) {
        std::cerr << "thread library error: " << "tid " << tid << " is not exist" << std::endl;
        unblock_timer_signal();
        return -1;
    }

    delete[] threads[tid]->stack;
    delete threads[tid];
    threads[tid] = nullptr;

    if (current_thread == tid) {
        unblock_timer_signal();
        context_switch();
    }
    unblock_timer_signal();
    return 0;
}


/**
 * @brief Blocks the thread with ID tid. The thread may be resumed later using uthread_resume.
 *
 * If no thread with ID tid exists it is considered as an error. In addition, it is an error to try blocking the
 * main thread (tid == 0). If a thread blocks itself, a scheduling decision should be made. Blocking a thread in
 * BLOCKED state has no effect and is *not* considered an error.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_block(int tid) {
    block_timer_signal();

    if (tid <= 0 || tid >= MAX_THREAD_NUM || threads[tid] == nullptr) {
        std::cerr << "thread library error: Invalid tid in uthread_block\n" << std::endl;
        return -1;
    }

    thread *t = threads[tid];
    if (t->is_actively_blocked) {
        unblock_timer_signal();
        return 0;
    }

    t->state = BLOCKED;
    t->is_actively_blocked = true;

    if (tid == current_thread) {
        unblock_timer_signal();
        context_switch();
    }

    unblock_timer_signal();
    return 0;
}



/**
 * @brief Resumes a blocked thread with ID tid and moves it to the READY state.
 *
 * Resuming a thread in a RUNNING or READY state has no effect and is not considered as an error. If no thread with
 * ID tid exists it is considered an error.
 * When a thread transition to the READY state it is placed at the end of the READY queue.
 *
 * @return On success, return 0. On failure, return -1.
*/

int uthread_resume(int tid) {

    block_timer_signal();

    if (tid < 0 || tid >= MAX_THREAD_NUM || threads[tid] == nullptr) {
        std::cerr << "thread library error: Invalid tid in uthread_resume\n" << std::endl;
        unblock_timer_signal();
        return -1;
    }
    thread *t = threads[tid];
    if (t->state == RUNNING || t->state == READY) {
        unblock_timer_signal();
        return 0;
    }
    t->is_actively_blocked = false;
    if (threads[tid]->state == BLOCKED && threads[tid]->quantum_sleep == 0) {
        threads[tid]->state = READY;
        ready_queue.push(tid);
    }

    unblock_timer_signal();
    return 0;
}



/**
 * @brief Blocks the RUNNING thread for num_quantums quantums.
 *
 * Immediately after the RUNNING thread transitions to the BLOCKED state a scheduling decision should be made.
 * After the sleeping time is over, the thread should go back to the end of the READY queue.
 * If the thread which was just RUNNING should also be added to the READY queue, or if multiple threads wake up 
 * at the same time, the order in which they're added to the end of the READY queue doesn't matter.
 * The number of quantums refers to the number of times a new quantum starts, regardless of the reason. Specifically,
 * the quantum of the thread which has made the call to uthread_sleep isn’t counted.
 * A call with num_quantums == 0 will immediately stop the thread and move it to the back of the execution queue.
 * 
 * It is considered an error if the main thread (tid == 0) calls this function with num_quantums != 0.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_sleep(int num_quantums) {

    block_timer_signal();
    if (num_quantums < 0) {
        std::cerr << "thread library error: " << "num_quantums can't be negative" << std::endl;
        unblock_timer_signal();
        return -1;
    }

    if (current_thread == 0 && num_quantums != 0) {
        std::cerr << "thread library error: " << "main thread can't sleep" << std::endl;
        unblock_timer_signal();
        return -1;
    }
    if (num_quantums == 0) {
        threads[current_thread]->state = READY;
        ready_queue.push(current_thread);
        context_switch();
        unblock_timer_signal();
        return 0;
    }
    threads[current_thread]->state = BLOCKED;
    threads[current_thread]->quantum_sleep = num_quantums+1;
    context_switch();

    unblock_timer_signal();
    return 0;
}


/**
 * @brief Returns the thread ID of the calling thread.
 *
 * @return The ID of the calling thread.
*/
int uthread_get_tid() {
    block_timer_signal();
    int res = current_thread;
    unblock_timer_signal();
    return res;
}


/**
 * @brief Returns the total number of quantums since the library was initialized, including the current quantum.
 *
 * Right after the call to uthread_init, the value should be 1.
 * Each time a new quantum starts, regardless of the reason, this number should be increased by 1.
 *
 * @return The total number of quantums.
*/
int uthread_get_total_quantums() {
    block_timer_signal();
    int res = quantum_counter;
    unblock_timer_signal();
    return res;
}


/**
 * @brief Returns the number of quantums the thread with ID tid was in RUNNING state.
 *
 * On the first time a thread runs, the function should return 1. Every additional quantum that the thread starts should
 * increase this value by 1 (so if the thread with ID tid is in RUNNING state when this function is called, include
 * also the current quantum). If no thread with ID tid exists it is considered an error.
 *
 * @return On success, return the number of quantums of the thread with ID tid. On failure, return -1.
*/
int uthread_get_quantums(int tid) {
    block_timer_signal();
    if (tid < 0 || tid >= MAX_THREAD_NUM) {
        std::cerr << "thread library error: " << "tid must be positive" << std::endl;
        unblock_timer_signal();
        return -1;
    }
    if (threads[tid] == nullptr) {
        std::cerr << "thread library error: " << "tid " << tid << " is not exist" << std::endl;
        unblock_timer_signal();
        return -1;
    }
    int res =  threads[tid]->quantum;
    unblock_timer_signal();
    return res;
}
