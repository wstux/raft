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

#ifndef _EXAMPLES_CLUSTER_NODE_H_
#define _EXAMPLES_CLUSTER_NODE_H_

#include <chrono>
#include <memory>
#include <thread>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Woverflow"
#pragma GCC diagnostic ignored "-Wunused-parameter"
    #include <grpc++/grpc++.h>
#pragma GCC diagnostic pop

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Woverflow"
    #include <node.pb.h>
    #include <node.grpc.pb.h>
#pragma GCC diagnostic pop

#include <raft/server.h>

#include "counter/details/db_guard.h"
#include "counter/details/fsm.h"
#include "counter/details/logging.h"

namespace wstux {
namespace examples {
namespace cluster {

class node : public NodeService::Service
{
public:
    using ptr = std::shared_ptr<node>;

public:
    node(const raft::logging_handler::severity_level level);

    virtual ~node();

    virtual ::grpc::Status SendRaftMessage(::grpc::ServerContext* p_ctx, const Message* p_req, Empty* p_resp) override;

    void apply(raft::buffer_type buffer) { m_p_server->apply(std::move(buffer)); }

    raft::buffer_type get_value() const { return m_p_fsm->get_buffer(); }

    int run();

    void stop();

protected:
    virtual raft::cluster_config bootstrap() const = 0;

    virtual raft::config config() const = 0;

    virtual bool execute_follower() = 0;

    virtual bool execute_leader() = 0;

    virtual std::string join_address() const = 0;

    virtual bool init() = 0;

    virtual raft::server_id_t server_id() const = 0;

    virtual std::string work_dir() const = 0;

private:
    enum server_state
    {
        ready,
        starting,
        stopped
    };

private:
    inline bool is_ready() const { return (m_state == server_state::ready); }

    inline bool is_stopped() const { return (m_state == server_state::stopped); }

    bool start_rpc(const std::string address);

    void stop_rpc();

    void thread_main_rpc(const std::string& address);

    template<class TRep, class TPeriod>
    void wait_for_rpc(const std::chrono::duration<TRep, TPeriod>& rel_time)
    {
        using type_point_t = std::chrono::time_point<std::chrono::system_clock>;

        const type_point_t tp = std::chrono::system_clock::now() + rel_time;
        while (! is_ready() && (std::chrono::system_clock::now() < tp)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

private:
    std::atomic_bool m_is_started{false};
    std::atomic<server_state> m_state{server_state::stopped};
    std::unique_ptr<::grpc::Server> m_p_rpc_server;
    std::unique_ptr<std::thread> m_thread;

    details::db_guard::ptr m_p_db_guard;
    details::fsm::ptr m_p_fsm;
    raft::server::ptr m_p_server;

    std::atomic_uint64_t m_counter;

    raft::logging_handler::severity_level m_level;
    logging_handler m_logger;
};

} // namespace cluster
} // namespace examples
} // namespace wstux

#endif /* _EXAMPLES_CLUSTER_NODE_H_ */
