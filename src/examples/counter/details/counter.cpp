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

#include <chrono>
#include <thread>

#include "counter/counter.h"

namespace wstux {
namespace examples {
namespace counter {
namespace {

raft::le::server::ptr make_server(const details::io::ptr& p_io, const config::ptr& p_cfg)
{
    raft::le::server::ptr p_srv;
    const std::function<bool()> is_stop_fn = []()->bool { return false; };
    raft::le::logging_handler::ptr p_logger = std::make_unique<details::logging_handler>(p_cfg->level());
    p_srv = std::make_shared<raft::le::server>(p_cfg->server_id(), p_io, std::move(p_logger), is_stop_fn);
    return p_srv;
}

} // <anonymous> namespace

counter_node::counter_node(const config::ptr& p_config)
    : m_p_config(p_config)
    , m_p_io(std::make_shared<details::io>(m_p_config->server_id(), m_p_config->cluster_config(), m_p_config->level()))
    , m_p_server(make_server(m_p_io, m_p_config))
    , m_counter(0)
    , m_logger(m_p_config->level())
{}

counter_node::~counter_node()
{
    stop();
}

int counter_node::run()
{
    if (! m_p_server->init()) {
        LOG_ERROR(m_logger, "Failed to init raft server");
        return 1;
    }
    if (! m_p_server->start()) {
        LOG_ERROR(m_logger, "Failed to start raft server");
        return 1;
    }

    if (! start_rpc(m_p_config->endpoint())) {
        LOG_ERROR(m_logger, "Failed to start rpc server");
        stop();
        return 1;
    }

    while (is_ready()) {
        if (m_p_server->is_leader()) {
            ++m_counter;
            m_p_io->update_counter(m_counter);
        }
        if (m_counter % 10 == 0) {
            LOG_INFO(m_logger, "Current counter value " << m_counter);
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}

bool counter_node::start_rpc(const std::string address)
{
    bool expected = false;
    if (! m_is_started.compare_exchange_strong(expected, true)) {
        return false;
    }

    const std::function<void()> thread_fn = [this, address]() -> void {
        try {
            thread_main_rpc(address);
        } catch (const std::exception& ex) {
            m_is_started = false;
        }
    };

    server_state expected_state = server_state::stopped;
    if (! m_state.compare_exchange_strong(expected_state, server_state::starting)) {
        LOG_ERROR(m_logger, "Incorrect server state " << m_state);
        return false;
    }
    m_thread = std::make_unique<std::thread>(thread_fn);
    wait_for_rpc(std::chrono::seconds(1));
    return is_ready();
}

void counter_node::stop()
{
    stop_rpc();
    m_p_server->stop();
}

void counter_node::stop_rpc()
{
    if (is_stopped()) {
        return;
    }
    m_state = server_state::stopped;
    if (m_p_rpc_server) {
        m_p_rpc_server->Shutdown();
    }

    if (m_thread && m_thread->joinable()) {
        m_thread->join();
    }
}

void counter_node::thread_main_rpc(const std::string& address)
{
    if (is_stopped()) {
        LOG_DEBUG(m_logger, "Server has been stopped");
        return;
    }

    ::grpc::ServerBuilder builder;
    builder.AddListeningPort(address, ::grpc::InsecureServerCredentials());
    builder.RegisterService(this);
    m_p_rpc_server = std::move(builder.BuildAndStart());
    if (m_p_rpc_server.get() != nullptr) {
        LOG_DEBUG(m_logger, "Server starts listening to address " << address);
    } else {
        LOG_WARN(m_logger, "Could not listen to address " << address);
    }

    if (m_p_rpc_server) {
        // Run server
        server_state expected_state = server_state::starting;
        if (! m_state.compare_exchange_strong(expected_state, server_state::ready)) {
            return;
        }
        m_p_rpc_server->Wait();
    }
}

::grpc::Status counter_node::SendRaftMessage(::grpc::ServerContext*, const ::cluster::Message* p_req, ::cluster::Empty*)
{
    LOG_TRACE(m_logger, "Got raft message.");

    raft::le::buffer_type msg(p_req->buffer().data(), p_req->buffer().size());
    m_p_server->handle_message(msg);
    return ::grpc::Status::OK;
}

::grpc::Status counter_node::SentCounterMessage(::grpc::ServerContext*, const ::cluster::CounterMessage* p_req, ::cluster::Empty*)
{
    LOG_DEBUG(m_logger, "Got counter message.");

    m_counter = p_req->counter();
    return ::grpc::Status::OK;
}

} // namespace counter
} // namespace examples
} // namespace wstux
