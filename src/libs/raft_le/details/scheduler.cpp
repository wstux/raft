/*
 * The MIT License
 *
 * Copyright 2025 Chistyakov Alexander.
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

#include <chrono>

#include "raft_le/details/scheduler.h"

namespace wstux {
namespace raft {
namespace le {
namespace details {

////////////////////////////////////////////////////////////////////////////////
// struct scheduler::task

/**
 *  \brief  Internal structure representing a scheduler task.
 *
 *  \details    Encapsulates a Boost.Asio timer, a cancellation flag, and
 *      a user functor.
 */
struct scheduler::task final
{
    /// \brief  Constructor.
    /// \param  h - user handler.
    /// \param  io - asio asynchronous context.
    task(scheduler::handler_type&& h, boost::asio::io_context& io)
        : is_cancelled(true)
        , exec(std::move(h))
        , timer(io)
    {}

    /// \brief  Executes the user functor if the task has not been canceled.
    void execute()
    {
        if (! is_cancelled.load()) {
            exec();
        }
    }

    /// \brief  Sets the task to the "canceled" state and interrupts the timer wait.
    void cancel()
    {
        is_cancelled.store(true);
        timer.cancel();
    }

    /// \brief  Resets the cancellation flag, preparing the task for execution.
    void resume() { is_cancelled.store(false); }

    /// @brief  Callback method for the asynchronous asio timer.
    /// @param  task - the task, extending its lifetime for the duration of the callback.
    /// @param  ec - error code returned by the operating system or the boost.asio library.
    static void handler(scheduler::task_type task, const boost::system::error_code& ec)
    {
        try {
            if (! ec) {
                task->execute();
            }
        } catch (const boost::thread_interrupted& /*ex*/) {
            // Catching thread execution interruption.
        } catch (const std::exception& /*ex*/) {
            // Catching any standard exceptions inside user code.
        }
    }

    std::atomic_bool is_cancelled;   ///< Flag indicating the cancellation/activity state of the task.
    scheduler::handler_type exec;    ///< User handler.
    boost::asio::steady_timer timer; ///< Timer for implementing execution delays.
};

////////////////////////////////////////////////////////////////////////////////
// class scheduler

scheduler::scheduler()
    : m_io_ctx()
    , m_strand(boost::asio::make_strand(m_io_ctx))
{}

scheduler::~scheduler()
{
    stop();
}

void scheduler::cancel(const task_type& task)
{
    if (task) {
        task->cancel();
    }
}

void scheduler::execute_async(handler_type&& handler)
{
    using asio_allocator_type = boost::asio::recycling_allocator<void>;

    if (m_is_stop.load(std::memory_order_acquire)) {
        return;
    }
    boost::asio::post(m_io_ctx, boost::asio::bind_allocator(asio_allocator_type(), std::move(handler)));
}

void scheduler::execute_strand(handler_type&& handler)
{
    using asio_allocator_type = boost::asio::recycling_allocator<void>;

    if (m_is_stop.load(std::memory_order_acquire)) {
        return;
    }
    boost::asio::post(m_strand, boost::asio::bind_allocator(asio_allocator_type(), std::move(handler)));
}

void scheduler::init(size_t pool_size)
{
    if (! m_is_stop.load(std::memory_order_acquire)) {
        return;
    }
    pool_size = thread_pool_size(pool_size);
    init_asio(pool_size);
}

void scheduler::init_asio(size_t pool_size)
{
    if (m_thread_pool.get() != nullptr) {
        stop_asio();
    }

    // Update size configuration
    m_threads_size.store(pool_size);
    if (m_threads_size == 0) {
        return;
    }

    m_work_guard = std::make_unique<work_guiard_t>(boost::asio::make_work_guard(m_io_ctx));
    m_thread_pool = std::make_unique<boost::asio::thread_pool>(m_threads_size);
}

bool scheduler::is_canceled(const task_type& task) const
{
    return task->is_cancelled.load();
}

scheduler::task_type scheduler::make_task(handler_type&& handler)
{
    return std::make_shared<task>(std::move(handler), m_io_ctx);
}

void scheduler::reconfigure(size_t new_size)
{
    if (m_is_stop.load(std::memory_order_acquire)) {
        return;
    }

    new_size = thread_pool_size(new_size);

    std::unique_lock<std::shared_mutex> lock(m_pool_mutex);

    if (m_threads_size == new_size) {
        return;
    }

    // Pool restart. Stop and destroy the thread_pool. All scheduled tasks remain inside io_ctx.
    stop_asio();

    // Restart io_ctx processing on the new pool threads
    init_asio(new_size);
    start_asio();
}

void scheduler::reschedule(const task_type& task, int32_t ms)
{
    cancel(task);
    schedule(task, ms);
}

void scheduler::schedule(const task_type& task, int32_t ms)
{
    using asio_allocator_type = boost::asio::recycling_allocator<void>;

    if (m_is_stop.load(std::memory_order_acquire)) {
        return;
    }

    std::shared_lock<std::shared_mutex> lock(m_pool_mutex);

    // Prevention of rescheduling an already active task
    if (! task || ! task->is_cancelled) {
        return;
    }

    task->resume();
    task->timer.expires_after(std::chrono::milliseconds(ms));
    task->timer.async_wait(
        boost::asio::bind_executor(
            m_strand,
            boost::asio::bind_allocator(
                asio_allocator_type(),
                [task](const boost::system::error_code& ec) { task::handler(std::move(task), ec); }
            )
        )
    );
}

void scheduler::start()
{
    bool expected = true;
    if (! m_is_stop.compare_exchange_strong(expected, false)) {
        return;
    }

    std::unique_lock<std::shared_mutex> lock(m_pool_mutex);
    start_asio();
}

void scheduler::start_asio()
{
    using asio_allocator_type = boost::asio::recycling_allocator<void>;

    // Restart io_ctx processing on the new pool threads
    for (size_t i = 0; i < m_threads_size; ++i) {
        boost::asio::post(*m_thread_pool, boost::asio::bind_allocator(asio_allocator_type(), [this]() { m_io_ctx.run(); }));
    }
}

void scheduler::stop()
{
    bool expected = false;
    if (! m_is_stop.compare_exchange_strong(expected, true)) {
        return;
    }

    std::unique_lock<std::shared_mutex> lock(m_pool_mutex);
    stop_asio();
}

void scheduler::stop_asio()
{
    if (m_thread_pool.get() == nullptr) {
        return;
    }
    m_work_guard.reset();

    // Stops the processing of new tasks in the pool immediately.
    m_io_ctx.stop();

    // Blocks the calling thread until all worker threads finish execution.
    m_thread_pool->stop();
    m_thread_pool->join();
    m_thread_pool.reset();

    m_io_ctx.restart();
}

size_t scheduler::thread_pool_size(size_t pool_size)
{
    if (! pool_size) {
        pool_size = std::thread::hardware_concurrency();
    }
    if (! pool_size) {
        pool_size = 1;
    }
    return pool_size;
}

size_t scheduler::threads_size() const
{
    return m_threads_size.load();
}

} // namespace details
} // namespace le
} // namespace raft
} // namespace wstux
