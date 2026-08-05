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

#ifndef _LIBS_RAFT_LEADER_ELECTION_SCHEDULER_H_
#define _LIBS_RAFT_LEADER_ELECTION_SCHEDULER_H_

#include <atomic>
#include <functional>
#include <memory>

namespace wstux {
namespace raft {
namespace le {
namespace details {

/**
 *  \brief  An asynchronous task scheduler class.
 *
 *  \details    Provides creation, execution, cancellation, and rescheduling of
 *      user-defined functions within a multi-threaded pool.
 */
class scheduler final
{
private:
    struct task;

public:
    using ptr = std::shared_ptr<scheduler>;     ///< Pointer to the scheduler instance.
    using handler_type = std::function<void()>; ///< Type of the user handler.
    using task_type = std::shared_ptr<task>;    ///< Task structure.

public:
    /// \brief  Constructor.
    /// \param  pool_size - number of asio worker threads. If 0 is passed, the
    ///     size is calculated automatically based on the hardware configuration.
    scheduler(size_t pool_size = 4);

    /// \brief  Destructor. Automatically calls the stop() method to terminate threads.
    ~scheduler();

    /// \brief  Cancels the execution of a scheduled task.
    /// \param  task - task to be canceled.
    void cancel(const task_type& task);

    /// \brief  Creates and schedules the immediate asynchronous execution of a new handler.
    /// \param  handler - the handler function to execute.
    void execute_async(const handler_type& handler);

    /// \brief  Executes the handler strictly sequentially within the strand.
    /// \param  handler - the handler function to execute.
    void execute_strand(const handler_type& handler);

    bool is_canceled(const task_type& task) const;

    /// \brief  Factory method to create a task object without starting it.
    /// \param  handler - the handler function to bind to the task.
    /// \return The created task.
    task_type make_task(const handler_type& handler) const;

    /// \brief  Dynamically changes the size of the worker thread pool.
    /// \param  new_size - the new size of the thread pool.
    /// \details    This method is thread-safe. Existing tasks in the io_context are not lost.
    void reconfigure(size_t new_size);

    /// \brief  Reschedules a task to a new time, canceling its current execution first.
    /// \param  task - task.
    /// \param  ms - delay before execution in milliseconds.
    void reschedule(const task_type& task, int32_t ms);

    /// \brief  Schedules a task to execute with a specified delay.
    /// \param  task - task.
    /// \param  ms - delay before execution in milliseconds.
    void schedule(const task_type& task, int32_t ms);

    void start();

    /// \brief  Immediately stops the thread pool and waits for threads to finish execution.
    /// \details    Tasks currently queued or waiting on a timer will be discarded.
    void stop();

    /// \brief  Returns the current number of worker threads.
    /// \return The current size of the thread pool.
    size_t threads_size() const;

private:
    /// \brief  Helper method to determine the actual thread pool size.
    /// \param  pool_size - requested pool size.
    /// \return The actual number of threads (at least 1).
    static size_t thread_pool_size(size_t pool_size);

private:
    struct context;

private:
    std::atomic_bool m_is_stop{true};
    std::unique_ptr<context> m_p_ctx; ///< Pointer to the execution context.
};

} // namespace details
} // namespace le
} // namespace raft
} // namespace wstux

#endif /* _LIBS_RAFT_LEADER_ELECTION_SCHEDULER_H_ */
