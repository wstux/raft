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

#ifndef _LIBS_RAFT_CONTEXT_H_
#define _LIBS_RAFT_CONTEXT_H_

#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <random>
#include <shared_mutex>
#include <string>
#include <type_traits>

#include "raft/io.h"
#include "raft/details/logger.h"
#include "raft/details/scheduler.h"
#include "raft/details/connection/peer.h"
#include "raft/details/log/log.h"
#include "raft/details/role/role.h"

namespace wstux {
namespace raft {
namespace details {
namespace process {

struct state final
{
    index_t commit_index;
    index_t last_applied;
    index_t last_stored;

    index_t configuration_committed_index;
    index_t configuration_uncommitted_index;

    struct {
        bool is_in_process = false;
    } snapshot;

    size_t tasks_in_process;
};

} // namespace process

struct context final: public std::enable_shared_from_this<context>
{
    using ptr = std::shared_ptr<context>;

    context(server_id_t id, const io::ptr p_io, const fsm::ptr p_fsm, logging_handler::ptr p_handler,
            const is_stop_fn_t& is_stop, const allocator_type& alloc = allocator_type());

    const server_id_t id;
    const is_stop_fn_t is_stop_fn;
    allocator_type alloc;

    bool is_async_io;

    server_config config;

    io::ptr p_io;
    fsm::ptr p_fsm;

    role::state role;
    process::state state;
    log::store log;

    term_t term;

    peer::list peers;

    scheduler schd;

    size_t snapshot_threshold;

    size_t heartbeat_interval_ms;
    scheduler::task_type heartbeat_task;

    std::mt19937 rand_engine;
    std::uniform_int_distribution<size_t> election_distribution;
    scheduler::task_type election_task;

    logger raft_logger;
};

std::ostream& operator<<(std::ostream& os, const context& ctx);

namespace peers {

/**
 *  \brief  Checks if the leader maintains active contact with a majority (quorum)
 *      of nodes.
 *  \param  ctx - current server state context.
 *  \return true if the leader has active contact with a quorum of voting nodes
 *      (including itself), otherwise false if the leader has lost contact with
 *      the quorum and must become a follower.
 *
 *  \details    Raft Paper, Section 6 (Cluster membership changes / Leader lease):
 *      "A leader steps down if it does not receive heartbeat responses from a
 *      majority of the cluster nodes within an election timeout period."
 *
 *      Used for Leader Lease management. If the leader detects that it
 *      is disconnected from the majority of cluster nodes, it must step down to
 *      prevent a split-brain scenario.
 */
bool check_contact_quorum(context& ctx);

peer::ptr find(context& ctx, server_id_t id);

size_t quorum_for_election(context& ctx);

void update(context& ctx, const cluster_config& cluster_cfg);

size_t voting_members_count(context& ctx);

} // namespace peers

namespace utils {

bool init(context& ctx);

bool is_valid_cluster(const server_id_t id, const cluster_config& cluster_cfg);

bool load(context& ctx);

void reconfigure(context& ctx, const config& cfg, const cluster_config& cluster_cfg);

} // namespace utils

} // namespace details
} // namespace raft
} // namespace wstux

#endif /* _LIBS_RAFT_CONTEXT_H_ */
