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

#include "counter/details/fsm.h"
#include "counter/details/io.h"
#include "counter/details/node.h"

namespace wstux {
namespace examples {
namespace cluster {
namespace {

raft::server::ptr make_server(const raft::server_id_t id, const details::db_guard::ptr& p_db_guard,
                              const details::fsm::ptr& p_fsm, const raft::logging_handler::severity_level lvl)
{
    raft::server::ptr p_srv;

    details::io::ptr  p_io = std::make_shared<details::io>(p_db_guard, lvl);

    const std::function<bool()> is_stop_fn = []()->bool { return false; };
    raft::logging_handler::ptr p_logger = std::make_unique<logging_handler>(lvl);
    p_srv = std::make_shared<raft::server>(id, p_io, p_fsm, std::move(p_logger), is_stop_fn);
    return p_srv;
}

} // <anonymous> namespace

node::node(const raft::logging_handler::severity_level level)
    : m_level(level)
    , m_logger(level)
{}

node::~node()
{
    stop();
}

int node::run()
{
    const  raft::server_id_t id = server_id();

    m_p_db_guard = std::make_shared<details::db_guard>();
    if (! m_p_db_guard->create_env(work_dir())) {
        LOG_ERROR(m_logger, "Failed to init storage");
        return 1;
    }

    m_p_fsm = std::make_shared<details::fsm>(m_p_db_guard);
    m_p_server = make_server(id, m_p_db_guard, m_p_fsm, m_level);

    if (! init()) {
        LOG_ERROR(m_logger, "Failed to init node");
        return 1;
    }

    raft::config cfg = config();
    raft::cluster_config cluster_cfg = bootstrap();

    if (! cluster_cfg.servers.empty()) {
        LOG_DEBUG(m_logger, "Counter starts with bootstrap and '" << cfg.address << "' endpoint");
        if (! m_p_server->bootstrap(cfg, cluster_cfg)) {
            LOG_ERROR(m_logger, "Failed to init bootstrap raft server");
            return 1;
        }
    } else {
        LOG_DEBUG(m_logger, "Counter starts with '" << cfg.address << "' endpoint");
        if (! m_p_server->init(cfg)) {
            LOG_ERROR(m_logger, "Failed to init raft server");
            return 1;
        }
    }

    if (! m_p_server->start()) {
        LOG_ERROR(m_logger, "Failed to start raft server");
        return 1;
    }

    if (! start_rpc(cfg.address)) {
        LOG_ERROR(m_logger, "Failed to start rpc server");
        stop();
        return 1;
    }

    const std::string join_addr = join_address();
    if (! join_addr.empty()) {
        LOG_DEBUG(m_logger, "Counter with '" << cfg.address << "' endpoint is joining to cluster " << join_addr);
        m_p_server->join(join_addr);
    }

    while (is_ready()) {
        bool rc = false;
        if (m_p_server->is_leader()) {
            rc = execute_leader();
        } else if (m_p_server->is_follower()) {
            rc = execute_follower();
        }
        if (! rc ) {
            stop();
            return 1;
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}

bool node::start_rpc(const std::string address)
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

void node::stop()
{
    stop_rpc();
    if (m_p_server) {
        m_p_server->stop();
    }
}

void node::stop_rpc()
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

void node::thread_main_rpc(const std::string& address)
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

::grpc::Status node::SendRaftMessage(::grpc::ServerContext*, const Message* p_req, Empty*)
{
    LOG_TRACE(m_logger, "Got raft message.");

    raft::inbuffer_type msg(p_req->buffer().data(), p_req->buffer().size());
    m_p_server->handle_message(msg);
    return ::grpc::Status::OK;
}

} // namespace cluster
} // namespace examples
} // namespace wstux
