#include "../uthreads.h"
#include <iostream>
#include <cassert>
#include <string>

/**
 * Helper function to print success messages for specific test cases.
 */
void print_success(const std::string& test_name) {
    std::cout << "[SUCCESS] " << test_name << std::endl;
}

/**
 * A simple thread function that runs in an infinite loop.
 */
void simple_thread() {
    while (true) {
        // Yielding execution is usually done via a timer,
        // but for this logic test, we stay in a loop.
    }
}

/**
 * A thread function that demonstrates self-blocking behavior.
 */
void self_blocking_thread() {
    int tid = uthread_get_tid();
    std::cout << "Thread " << tid << " is now blocking itself..." << std::endl;

    // This call should trigger a context switch
    uthread_block(tid);

    // Execution will only reach here if another thread calls uthread_resume(tid)
    std::cout << "Thread " << tid << " has been resumed. Terminating now." << std::endl;
    uthread_terminate(tid);
}

int main() {
    // 1. Initialize the library
    if (uthread_init(100) != 0) {
        std::cerr << "Library initialization failed" << std::endl;
        return 1;
    }
    print_success("uthread_init");

    // --- Error Handling & Edge Case Tests ---

    // 2. Attempt to block the Main Thread (TID 0)
    // According to requirements, this should return an error (-1).
    assert(uthread_block(0) == -1);
    print_success("Block main thread (TID 0) fails as expected");

    // 3. Attempt to block or resume invalid TIDs
    // Tests for negative IDs, IDs exceeding MAX_THREAD_NUM, and non-existent threads.
    assert(uthread_block(99) == -1);
    assert(uthread_block(-5) == -1);
    assert(uthread_resume(99) == -1);
    print_success("Invalid TID handling (out of range or non-existent)");

    // --- Block/Resume Logic Tests ---

    // 4. Spawn a new thread (should receive TID 1)
    int tid1 = uthread_spawn(simple_thread);
    assert(tid1 == 1);
    print_success("Spawned thread with TID 1");

    // 5. Block a thread that is currently in the READY state
    assert(uthread_block(tid1) == 0);
    print_success("Blocking a READY thread");

    // 6. Block a thread that is already BLOCKED
    // This should have no effect and return 0 (success).
    assert(uthread_block(tid1) == 0);
    print_success("Blocking an already BLOCKED thread (no-op)");

    // 7. Resume a BLOCKED thread
    // This should move the thread back to the READY queue.
    assert(uthread_resume(tid1) == 0);
    print_success("Resuming a BLOCKED thread");

    // 8. Resume a thread that is already READY or RUNNING
    // This should have no effect and return 0 (success).
    assert(uthread_resume(tid1) == 0);
    print_success("Resuming a READY thread (no-op)");

    // --- Advanced Edge Case: Termination while Blocked ---

    // 9. Terminate a thread while it is in the BLOCKED state
    int tid2 = uthread_spawn(simple_thread);
    uthread_block(tid2);
    assert(uthread_terminate(tid2) == 0);

    // Verify that the thread is truly gone (resume should now fail)
    assert(uthread_resume(tid2) == -1);
    print_success("Terminating a blocked thread and verifying its removal");

    // 10. Verify Quantum Counters Access
    // Total quantum should be at least 1 (for the main thread).
    int total = uthread_get_total_quantums();
    assert(total >= 1);
    print_success("Quantum counters are accessible and valid");

    std::cout << "\n--- All Block/Resume logic tests passed successfully! ---" << std::endl;

    return 0;
}
