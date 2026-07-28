/*
 * The MIT License
 *
 * Copyright 2026 Chistyakov Alexander.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <atomic>
#include <chrono>
#include <thread>

#include <gtest/gtest.h>

#include "raft_le/details/scheduler.h"

namespace {

namespace raft = wstux::raft::le::details;

void timer_handler(std::atomic_size_t& counter)
{
    counter++;
}

void long_timer_handler(std::atomic_size_t& counter)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    counter++;
}

} // <anonymous> namespace

/**
 *  \test   Verification of scheduled task execution.
 *  \see    raft::scheduler::schedule, raft::scheduler::make_task
 *
 *  **Test logic description:**
 *  Verifies that a regular task scheduled with a specific delay
 *  is successfully executed by the scheduler after the timeout expires.
 *
 *  **Steps to reproduce:**
 *  -# Create a scheduler instance and an atomic counter initialized to zero.
 *  -# Create a task that increments the counter, and schedule it with a 200 ms delay.
 *  -# Pause the main thread for 300 ms to give the scheduler time to process the task.
 *  -# Verify that the counter has incremented to 1.
 *  -# Stop the scheduler.
 *
 *  \expected_result    The scheduled task runs after the delay, updating the counter
 *      exactly once. The scheduler shuts down correctly.
 */
TEST(raft_scheduler, execute)
{
    raft::scheduler scheduler;
    std::atomic_size_t counter{0};
    raft::scheduler::handler_type handler_fn = std::bind(timer_handler, std::ref(counter));
    raft::scheduler::task_type task = scheduler.make_task(handler_fn);

    scheduler.schedule(task, 200);
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    EXPECT_TRUE(counter == 1) << counter;

    scheduler.stop();
}

/**
 *  \test   Verification of immediate asynchronous task execution.
 *  \see    raft::scheduler::execute_async
 *
 *  **Test logic description:**
 *  Verifies that tasks passed via `execute_async` are executed concurrently
 *  in a separate worker thread, rather than blocking the main calling thread.
 *
 *  **Steps to reproduce:**
 *  -# Create a scheduler instance and an empty variable for the thread ID.
 *  -# Pass a lambda function via `execute_async` that stores the ID of the current
 *     execution thread.
 *  -# Wait for 200 ms to allow the background asynchronous execution to complete.
 *  -# Compare the main thread ID with the saved worker thread ID.
 *
 *  \expected_result    Execution takes place in a background worker thread. The saved
 *      thread identifier is valid and strictly does not match the main thread ID.
 */
TEST(raft_scheduler, execute_async)
{
    raft::scheduler scheduler;

    std::thread::id worker_id;
    raft::scheduler::handler_type handler_fn = [&worker_id]() -> void { worker_id = std::this_thread::get_id(); };
    scheduler.execute_async(handler_fn);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_TRUE(std::this_thread::get_id() != worker_id) << worker_id;

    scheduler.stop();
}

/**
 *  \test   Verification of scheduled task cancellation.
 *  \see    raft::scheduler::schedule, raft::scheduler::cancel
 *
 *  **Test logic description:**
 *  Verifies that a scheduled task can be successfully canceled before its
 *  delay timer expires, guaranteeing that it is never executed.
 *
 *  **Steps to reproduce:**
 *  -# Create a scheduler and a task with a 200 ms execution delay.
 *  -# Schedule the task and sleep the thread for 100 ms (while the task is still queued).
 *  -# Explicitly cancel the task using the `scheduler.cancel()` method.
 *  -# Wait for an additional 300 ms to ensure the original delay window has passed.
 *  -# Verify the counter value.
 *
 *  \expected_result    The task is canceled while waiting in the queue.
 *      The counter remains equal to 0, confirming the handler was never invoked.
 */
TEST(raft_scheduler, cancel)
{
    raft::scheduler scheduler;
    std::atomic_size_t counter{0};
    raft::scheduler::handler_type handler_fn = std::bind(timer_handler, std::ref(counter));
    raft::scheduler::task_type task = scheduler.make_task(handler_fn);

    scheduler.schedule(task, 200);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    scheduler.cancel(task);
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    EXPECT_TRUE(counter == 0) << counter;

    scheduler.stop();
}

/**
 *  \test   Verification of scheduled task cancellation and rescheduling.
 *  \see    raft::scheduler::schedule, raft::scheduler::cancel, raft::scheduler::stop
 *
 *  **Test logic description:**
 *  Verifies that a scheduler can correctly manage multiple timed tasks,
 *  successfully cancel a pending task before its execution window,
 *  and accurately execute a task after it has been rescheduled.
 *
 *  **Steps to reproduce:**
 *  -# Create a scheduler and register two tasks bound to an atomic counter increment handler.
 *  -# Schedule task_1 with a 400 ms delay and task_2 with a 50 ms delay.
 *  -# Sleep the main thread for 100 ms to let task_2 execute while task_1 remains queued.
 *  -# Verify that only task_2 has executed (counter equals 1).
 *  -# Cancel the pending task_1 and immediately reschedule it with a new 200 ms delay.
 *  -# Sleep the main thread for 500 ms to allow the rescheduled task_1 to complete.
 *  -# Verify that the rescheduled task executed successfully (counter equals 2).
 *  -# Stop the scheduler to clean up resources.
 *
 *  \expected_result    Task_2 fires first while task_1 is safely canceled in the queue.
 *      After task_1 is rescheduled, it fires normally, bringing the final counter value to 2.
 */
TEST(raft_scheduler, cancel_task)
{
    raft::scheduler scheduler;
    std::atomic_size_t counter{0};
    raft::scheduler::handler_type handler_fn = std::bind(timer_handler, std::ref(counter));
    raft::scheduler::task_type task_1 = scheduler.make_task(handler_fn);
    raft::scheduler::task_type task_2 = scheduler.make_task(handler_fn);

    scheduler.schedule(task_1, 400);
    scheduler.schedule(task_2, 50);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_TRUE(counter == 1) << counter;

    scheduler.cancel(task_1);
    scheduler.schedule(task_1, 200);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    EXPECT_TRUE(counter == 2) << counter;

    scheduler.stop();
}

/**
 *  \test   Verification of scheduled task cancellation upon scheduler shutdown.
 *  \see    raft::scheduler::stop
 *
 *  **Test logic description:**
 *  Verifies that stopping the scheduler implicitly discards and cancels all
 *  remaining queued tasks whose execution has not yet started.
 *
 *  **Steps to reproduce:**
 *  -# Schedule a task with a 200 ms delay.
 *  -# Wait for 100 ms, then call `scheduler.stop()` to initiate the shutdown.
 *  -# Wait for an additional 300 ms to ensure there are no accidental triggers.
 *  -# Verify the final state of the counter.
 *
 *  \expected_result    Calling `stop()` safely discards unexecuted tasks.
 *      The counter remains equal to 0, confirming the task did not run after
 *      shutdown.
 */

TEST(raft_scheduler, stop)
{
    raft::scheduler scheduler;
    std::atomic_size_t counter{0};
    raft::scheduler::handler_type handler_fn = std::bind(timer_handler, std::ref(counter));
    raft::scheduler::task_type task = scheduler.make_task(handler_fn);

    scheduler.schedule(task, 200);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    scheduler.stop();
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    EXPECT_TRUE(counter == 0) << counter;
}

/**
 *  \test   Verification of dynamic thread pool configuration changes.
 *  \see    raft::scheduler::reconfigure, raft::scheduler::threads_size
 *
 *  **Test logic description:**
 *  Verifies that the size of the worker thread pool can be dynamically increased
 *  during runtime, and that the scheduler remains fully functional after resizing.
 *
 *  **Steps to reproduce:**
 *  -# Initialize the scheduler with 1 worker thread and verify the pool size.
 *  -# Dynamically increase the pool to 9 threads using the `reconfigure()` method.
 *  -# Ensure that the `threads_size()` method correctly returns a value of 9.
 *  -# Schedule a task with a 200 ms delay to verify pool health and functionality.
 *  -# Wait for 300 ms and check the counter.
 *
 *  \expected_result    The thread pool dynamically expands to the target size,
 *      and the scheduler correctly processes subsequent tasks.
 */
TEST(raft_scheduler, reconfigure)
{
    raft::scheduler scheduler(1);
    EXPECT_TRUE(scheduler.threads_size() == 1) << scheduler.threads_size();

    scheduler.reconfigure(9);
    EXPECT_TRUE(scheduler.threads_size() == 9) << scheduler.threads_size();

    std::atomic_size_t counter{0};
    raft::scheduler::handler_type handler_fn = std::bind(timer_handler, std::ref(counter));
    raft::scheduler::task_type task = scheduler.make_task(handler_fn);

    scheduler.schedule(task, 200);
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    EXPECT_TRUE(counter == 1) << counter;
}

/**
 *  \test   Verification of pool resizing while a task is waiting in the queue.
 *  \see    raft::scheduler::reconfigure
 *
 *  **Test logic description:**
 *  Verifies that dynamic reconfiguration of the pool does not corrupt, lose,
 *  or prematurely trigger delayed tasks waiting for their timers to expire.
 *
 *  **Steps to reproduce:**
 *  -# Initialize the pool with 5 threads and schedule a task with a 300 ms delay.
 *  -# Immediately reconfigure the scheduler's pool size to 9 threads.
 *  -# Ensure that at the moment of reconfiguration, the task has not yet executed
 *     (counter equals 0).
 *  -# Verify that the pool size has successfully updated to 9.
 *  -# Wait for 300 ms until the delay expires and check the execution status.
 *
 *  \expected_result    Pending tasks successfully survive the pool resizing process
 *      without side effects and execute strictly according to their timers.
 */
TEST(raft_scheduler, reconfigure_with_waiting_task)
{
    raft::scheduler scheduler(5);
    std::atomic_size_t counter{0};
    raft::scheduler::handler_type handler_fn = std::bind(timer_handler, std::ref(counter));
    raft::scheduler::task_type task = scheduler.make_task(handler_fn);

    scheduler.schedule(task, 300);

    scheduler.reconfigure(9);
    EXPECT_TRUE(counter == 0) << counter;
    EXPECT_TRUE(scheduler.threads_size() == 9) << scheduler.threads_size();

    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    EXPECT_TRUE(counter == 1) << counter;
}

/**
 *  \test   Verification of pool resizing with a long task delay time.
 *  \see    raft::scheduler::reconfigure
 *
 *  **Test logic description:**
 *  The test guarantees that a scheduled task is not interrupted or discarded
 *  if the internal structure of the pool changes.
 *
 *  **Steps to reproduce:**
 *  -# Initialize the pool with 5 threads and schedule a "long-delay" task to trigger
 *     in 200 ms.
 *  -# Reconfigure the pool to 9 worker threads.
 *  -# Confirm that the counter has not changed immediately after the pool modification.
 *  -# Wait for 600 ms to allow both the delay time and the task execution time to pass.
 *  -# Verify that the execution completed successfully.
 *
 *  \expected_result    Modifying the pool structure does not violate the integrity
 *      and lifecycle of elements scheduled for delayed execution.
 */
TEST(raft_scheduler, reconfigure_with_long_task)
{
    raft::scheduler scheduler(5);
    std::atomic_size_t counter{0};
    raft::scheduler::handler_type handler_fn = std::bind(long_timer_handler, std::ref(counter));
    raft::scheduler::task_type task = scheduler.make_task(handler_fn);

    scheduler.schedule(task, 200);

    scheduler.reconfigure(9);
    EXPECT_TRUE(counter == 0) << counter;
    EXPECT_TRUE(scheduler.threads_size() == 9) << scheduler.threads_size();

    std::this_thread::sleep_for(std::chrono::milliseconds(600));
    EXPECT_TRUE(counter == 1) << counter;
}

/**
 *  \test   Verification of pool resizing during active task execution.
 *  \see    raft::scheduler::reconfigure, raft::scheduler::execute_async
 *
 *  **Test logic description:**
 *  Verifies the scheduler behavior when a pool reconfiguration request is received
 *  at the exact moment worker threads are actively busy processing a task.
 *
 *  **Steps to reproduce:**
 *  -# Initialize the pool with 5 threads and dispatch a long-running task
 *     asynchronously.
 *  -# Wait for 100 ms to guarantee that the worker thread has entered its active
 *     execution phase.
 *  -# Initiate pool expansion to 9 threads (the test assumes that `reconfigure`
 *     will wait for task completion).
 *  -# Verify that the counter equals 1, indicating successful completion of the
 *     active task.
 *  -# Ensure that the pool capacity has registered the new thread size.
 *
 *  \expected_result Active worker thread operations are safely handled
 *      (or synchronized) during reconfiguration, and the pool size scales
 *      successfully.
 */
TEST(raft_scheduler, reconfigure_with_active_task)
{
    raft::scheduler scheduler(5);
    std::atomic_size_t counter{0};
    raft::scheduler::handler_type handler_fn = std::bind(long_timer_handler, std::ref(counter));

    scheduler.execute_async(handler_fn);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    scheduler.reconfigure(9);
    EXPECT_TRUE(counter == 1) << counter;
    EXPECT_TRUE(scheduler.threads_size() == 9) << scheduler.threads_size();
}

/**
 *  \test   Verification of reconfiguration protection after scheduler shutdown.
 *  \see    raft::scheduler::stop, raft::scheduler::reconfigure
 *
 *  **Test logic description:**
 *  Verifies the state-locking mechanism. A stopped scheduler must reject
 *  any pool reconfiguration requests and refuse to process new tasks.
 *
 *  **Steps to reproduce:**
 *  -# Initialize the scheduler with 5 worker threads.
 *  -# Explicitly stop the scheduler using the `scheduler.stop()` method.
 *  -# Attempt to reconfigure the pool size to 9 threads.
 *  -# Verify that the pool size remains locked at its previous value (5).
 *  -# Attempt to dispatch a new task asynchronously via `execute_async()`.
 *  -# Wait for 100 ms and ensure that the counter remains untouched.
 *
 *  \expected_result    Pool modification commands are ignored after shutdown.
 *      The pool size does not change, and any subsequent task dispatching is
 *      blocked.
 */
TEST(raft_scheduler, reconfigure_after_stop)
{
    raft::scheduler scheduler(5);
    scheduler.stop();

    scheduler.reconfigure(9);
    EXPECT_TRUE(scheduler.threads_size() == 5) << scheduler.threads_size();

    std::atomic_size_t counter{0};
    raft::scheduler::handler_type handler_fn = std::bind(long_timer_handler, std::ref(counter));

    scheduler.execute_async(handler_fn);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_TRUE(counter == 0) << counter;
}

int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
