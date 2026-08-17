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

#ifndef _EXAMPLES_RAFT_LEADER_ELECTION_COUNTER_COUNTER_H_
#define _EXAMPLES_RAFT_LEADER_ELECTION_COUNTER_COUNTER_H_

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>

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

#include "raft_le/io.h"
#include "raft_le/server.h"

#include "counter/config.h"
#include "counter/details/client.h"
#include "counter/details/io.h"
#include "counter/details/logging.h"

namespace wstux {
namespace examples {
namespace counter {

class counter_node final : public ::cluster::NodeService::Service
{
public:
    explicit counter_node(const config::ptr& p_config);

    ~counter_node();

    virtual ::grpc::Status SendRaftMessage(::grpc::ServerContext* p_ctx, const ::cluster::Message* p_req, ::cluster::Empty* p_resp) override;

    virtual ::grpc::Status SentCounterMessage(::grpc::ServerContext* p_ctx, const ::cluster::CounterMessage* p_req, ::cluster::Empty* p_resp) override;

    int run();

    void stop();

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
    config::ptr m_p_config;

    std::atomic_bool m_is_started{false};
    std::atomic<server_state> m_state{server_state::stopped};
    std::unique_ptr<::grpc::Server> m_p_rpc_server;
    std::unique_ptr<std::thread> m_thread;

    details::io::ptr m_p_io;
    raft::le::server::ptr m_p_server;

    std::atomic_uint64_t m_counter;

    details::logging_handler m_logger;
};

} // namespace counter
} // namespace examples
} // namespace wstux

#endif /* _EXAMPLES_RAFT_LEADER_ELECTION_COUNTER_COUNTER_H_ */
