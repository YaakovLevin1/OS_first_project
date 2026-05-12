#include "uthreads.h"
#include <iostream>
#include <limits>

void delete_another_thread() {
    int tid = uthread_get_tid();
    if (tid == -1) {
        std::cout << "Failed to get tid" << std::endl;
        exit(-1);
    }
    std::cout << "Thread " << tid << " is active" << std::endl;
    std::cout << tid << " trying to delete 1" << std::endl;
    if (uthread_terminate(1) == -1) {
        std::cout << "Failed to terminate 1" << std::endl;
        exit(-1);
    }
    if (tid != 0) {
        if (uthread_terminate(tid) != 0) {
            std::cout << "Failed to terminate" << std::endl;
            exit(-1);
        }
    }
}

void print_and_yield() {
    int tid = uthread_get_tid();
    if (tid == -1) {
        std::cout << "Failed to get tid" << std::endl;
        exit(-1);
    }
    std::cout << "Thread " << tid << " is active" << std::endl;
    uthread_sleep(0);
    if (tid != 0) {
        if (uthread_terminate(tid) != 0) {
            std::cout << "Failed to terminate" << std::endl;
            exit(-1);
        }
    }

}

int main(int argc, char **argv) {
    int result;
    result = uthread_init(std::numeric_limits<int>::max());
    if (result != 0) {
        std::cout << "uthread_init failed with code " << result << std::endl;
        exit(-1);
    }

    std::cout << "Creating 2 threads..." << std::endl;
    result = uthread_spawn(print_and_yield);
    if (result == -1) {
        std::cout << "Failed to create thread 1" << std::endl;
        exit(-1);
    }
    result = uthread_spawn(delete_another_thread);
    if (result == -1) {
        std::cout << "Failed to create thread 2" << std::endl;
        exit(-1);
    }

    uthread_sleep(0);

    std::cout << "Done" << std::endl;
    uthread_terminate(0);
}
