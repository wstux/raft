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

#ifndef _LIBS_RAFT_LEADER_ELECTION_CONTEXT_H_
#define _LIBS_RAFT_LEADER_ELECTION_CONTEXT_H_

#include <atomic>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <random>
#include <shared_mutex>
#include <string>
#include <type_traits>

#include "raft_le/io.h"
#include "raft_le/details/logger_channels.h"
#include "raft_le/details/scheduler.h"
#include "raft_le/details/connection/peer.h"
#include "raft_le/details/role/role.h"

namespace wstux {
namespace raft {
namespace le {
namespace details {

struct context final : public std::enable_shared_from_this<context>
{
    using ptr = std::shared_ptr<context>;

    context(server_id_t id, const io::ptr p_io, const ilogger_factory::ptr p_factory, const is_stop_fn_t& is_stop);

    const server_id_t id;
    const is_stop_fn_t is_stop_fn;

    bool is_async_io;

    server_config config;

    io::ptr p_io;

    std::mutex handler_mutex;
    role::state role;

    std::atomic<term_t> term;

    std::shared_mutex peers_mutex;
    peer::map peers;

    scheduler::ptr p_scheduler;

    size_t heartbeat_interval_ms;
    size_t heartbeat_probes_count;
    scheduler::task_type heartbeat_task;

    std::mt19937 rand_engine;
    std::uniform_int_distribution<size_t> election_distribution;
    scheduler::task_type election_task;

    loggers l;
};

std::ostream& operator<<(std::ostream& os, const context& ctx);

namespace peers {

bool emplace(context& ctx, const server_config& cfg);

peer::ptr find(context& ctx, server_id_t id);

size_t quorum_for_election(context& ctx);

size_t size(context& ctx);

void swap(context& ctx, peer::map& peers);

peer::list to_list(context& ctx);

size_t voting_members_count(context& ctx);

} // namespace peers

namespace sturtup {

bool init(context& ctx, server_id_t id);

bool load(context& ctx);

} // namespace sturtup

namespace utils {

size_t current_time_ms();

cluster_config_t make_config(context& ctx);

template<typename TFn, typename... TArgs>
inline typename std::result_of<TFn(context&, TArgs...)>::type wrap(context::ptr p_ctx, const TFn& func, TArgs&&... args)
{
    context& ctx = *p_ctx;
    return (*func)(ctx, std::forward<TArgs>(args)...);
}

} // namespace utils

} // namespace details
} // namespace le
} // namespace raft
} // namespace wstux

#endif /* _LIBS_RAFT_LEADER_ELECTION_CONTEXT_H_ */
