#include "uthreads.h"
#include <iostream>
#include <limits>

void sleeping_victim() {
    int tid = uthread_get_tid();
    std::cout << "Thread " << tid << " going to sleep for 2 quantums..." << std::endl;
    uthread_sleep(2);

    std::cout << "Thread " << tid << " finally WOKE UP!" << std::endl;
    uthread_terminate(tid);
}

void evil_blocker() {
    int tid = uthread_get_tid();
    std::cout << "Thread " << tid << " is blocking Thread 1 while it sleeps!" << std::endl;
    uthread_block(1);

    for (int i = 0; i < 4; ++i) {
        std::cout << "Thread " << tid << " passing time..." << std::endl;
        uthread_sleep(0);
    }

    std::cout << "Thread " << tid << " is now resuming Thread 1." << std::endl;
    uthread_resume(1);

    uthread_sleep(0);
    uthread_terminate(tid);
}

int main() {
    uthread_init(std::numeric_limits<int>::max());

    std::cout << "Creating threads..." << std::endl;
    uthread_spawn(sleeping_victim);
    uthread_spawn(evil_blocker);

    for (int i = 0; i < 8; ++i) {
        uthread_sleep(0);
    }

    std::cout << "Done" << std::endl;
    uthread_terminate(0);
}