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

#include <cassert>
#include <algorithm>
#include <mutex>

#include "raft/details/context.h"
#include "raft/details/connection/serialization.h"
#include "raft/details/replication/snapshot.h"

namespace wstux {
namespace raft {
namespace details {
namespace {

bool load_peers(context& ctx, cluster_config& cluster_cfg)
{
    std::sort(cluster_cfg.servers.begin(), cluster_cfg.servers.end(),
        [](const server_config& l, const server_config& r) -> bool { return l.id < r.id; });

    if (! utils::is_valid_cluster(ctx.id, cluster_cfg)) {
        return false;
    }

    for (const server_config& cfg : cluster_cfg.servers) {
        if (ctx.id != cfg.id) {
            assert(peers::find(ctx, cfg.id) == nullptr);
            ctx.peers.emplace_back(cfg);
        } else {
            ctx.config = cfg;
            ctx.role.is_voter = cfg.is_voter;
        }
    }
    return true;
}

bool restore_entries(context& ctx, index_t snapshot_index, term_t snapshot_term, index_t start_index, const entry::list& entries)
{
    index_t conf_index = 0;
    entry::ptr p_conf_entry;
    ctx.log.load(snapshot_index, snapshot_term, start_index);
    ctx.state.last_stored = start_index - 1;
    for (size_t i = 0; i < entries.size(); ++i) {
        entry::ptr p_entry = entries[i];
        ctx.log.append(p_entry);
        ++ctx.state.last_stored;

        // Only take into account configurations that are newer than the configuration restored from the snapshot.
        if (p_entry->type == entry_type::change && ctx.state.last_stored > ctx.state.configuration_committed_index) {
            if (conf_index != 0) {
                ctx.state.configuration_committed_index = conf_index;
            }
            p_conf_entry = p_entry;
            conf_index = ctx.state.last_stored;
        }
    }

    if (p_conf_entry) {
        cluster_config cluster_cfg = deserialize<cluster_config>(p_conf_entry->buffer);
        if (! load_peers(ctx, cluster_cfg)) {
            return false;
        }
    }
    return true;
}

} // <anonymous> namespace

////////////////////////////////////////////////////////////////////////////////
// class context

context::context(server_id_t id, const io::ptr p_io, const fsm::ptr p_fsm, logging_handler::ptr p_handler,
                 const is_stop_fn_t& is_stop, const allocator_type& alloc)
    : id(id)
    , is_stop_fn(is_stop)
    , alloc(alloc)
    , is_async_io(false)
    , config(gk_invalid_id, false)
    , p_io(p_io)
    , p_fsm(p_fsm)
    , term(0)
    , schd(alloc)
    , heartbeat_interval_ms(100)
    , rand_engine(std::chrono::system_clock::now().time_since_epoch().count() * id)
    , election_distribution(250, 500)
    , raft_logger(std::move(p_handler))
{}

std::ostream& operator<<(std::ostream& os, const context& ctx)
{
    os << ctx.id << "(" << ctx.role.str() << ")";
    return os;
}

namespace peers {

bool check_contact_quorum(context& ctx)
{
    assert(ctx.role.is_leader());

    size_t contacts = 1;
    size_t voting_count = 1;
    for (peer& p : ctx.peers) {
        const bool recent_recv = p.reset_recent_recv();
        contacts += (p.is_voter && recent_recv) ? 1 : 0;
        voting_count += (p.is_voter) ? 1 : 0;
    }
    const size_t quorum_for_election_size = (voting_count / 2);
    return contacts > quorum_for_election_size;
}

peer::ptr find(context& ctx, server_id_t id)
{
    peer::list::iterator it = std::find_if(ctx.peers.begin(), ctx.peers.end(), [id](const peer& p) { return p.id == id; });
    if (it != ctx.peers.cend()) {
        return &(*it);
    }
    return peer::ptr();
}

size_t quorum_for_election(context& ctx)
{
    const size_t members_count = voting_members_count(ctx);
    return (members_count / 2);
}

void update(context& ctx, const cluster_config& cluster_cfg)
{
    ctx.peers.clear();
    ctx.peers.reserve(std::max(ctx.peers.capacity(), cluster_cfg.servers.size()));

    for (const server_config& cfg : cluster_cfg.servers) {
        if (ctx.id != cfg.id) {
            assert(peers::find(ctx, cfg.id) == nullptr);
            ctx.peers.emplace_back(cfg);
        } else {
            ctx.config = cfg;
            ctx.role.is_voter = cfg.is_voter;
        }
    }
}

size_t voting_members_count(context& ctx)
{
    assert(ctx.role.is_voter);
    return 1 + std::count_if(ctx.peers.cbegin(), ctx.peers.cend(),
        [](const peer& p) -> bool { return p.is_voter; });
}

} // namespace peers

namespace utils {

bool init(context& ctx)
{
    if (! ctx.p_io->init(ctx.id)) {
        return false;
    }

    const config cfg = ctx.p_io->configuration();
    if (cfg.heartbeat_interval_ms == 0 || cfg.vote_timeout_max_ms == 0 || cfg.vote_timeout_max_ms < cfg.vote_timeout_min_ms) {
        return false;
    }

    ctx.is_async_io = cfg.is_async_io;

    ctx.role.voted_for = gk_invalid_id;

    ctx.schd.init(cfg.scheduler_threads_count);

    ctx.election_distribution = std::uniform_int_distribution<size_t>(cfg.vote_timeout_min_ms, cfg.vote_timeout_max_ms);
    ctx.heartbeat_interval_ms = cfg.heartbeat_interval_ms;

    // Reserve memory. Statistically, the cluster has less than or equal to 32
    // nodes. Therefore, memory is reserved for 32 nodes. If more is needed,
    // just reallocation will occur.
    ctx.peers.reserve(32);

    ctx.state.commit_index = 0;
    ctx.state.last_applied = 0;
    ctx.state.last_stored = 0;
    ctx.state.tasks_in_process = 0;

    return true;
}

bool is_valid_cluster(const server_id_t id, const cluster_config& cluster_cfg)
{
    assert(std::is_sorted(cluster_cfg.servers.cbegin(), cluster_cfg.servers.cend(),
        [](const server_config& l, const server_config& r) -> bool { return l.id < r.id; }));

    std::vector<server_config>::const_iterator it =
        std::adjacent_find(cluster_cfg.servers.cbegin(), cluster_cfg.servers.cend(),
            [](const server_config& l, const server_config& r) { return l.id == r.id; });
    if (it != cluster_cfg.servers.cend()) {
        return false;
    }
    it = std::find_if(cluster_cfg.servers.cbegin(), cluster_cfg.servers.cend(),
        [id](const server_config& cfg) -> bool { return cfg.id == id; });
    return it != cluster_cfg.servers.cend();
}

bool load(context& ctx)
{
    if (! ctx.peers.empty()) {
        return false;
    }

    io::ptr p_io = ctx.p_io;

    ctx.term = p_io->load_term();
    ctx.role.voted_for = p_io->voted_for();

    index_t snapshot_index = p_io->load_snapshot_index();
    term_t snapshot_term = p_io->load_snapshot_term();
    index_t start_index = p_io->load_start_index();

    snapshot::ptr p_snapshot = p_io->get_snapshot();
    entry::list entries = p_io->load_entries();
    if (p_snapshot.get() != nullptr) {
        if (! replication::snapshot::restore(ctx, *p_snapshot)) {
            return false;
        }
        snapshot_index = p_snapshot->index;
        snapshot_term = p_snapshot->term;
    } else if (entries.size() > 0) {
        assert(start_index == 1);
        assert(entries[0]->type == entry_type::change);

        ctx.state.commit_index = 1;
        ctx.state.last_applied = 1;
    }

    if (! restore_entries(ctx, snapshot_index, snapshot_term, start_index, entries)) {
        return false;
    }
    if (ctx.peers.empty()) {
        cluster_config cluster_cfg = p_io->bootstrap();
        if (! load_peers(ctx, cluster_cfg)) {
            return false;
        }
    }
    return true;
}

/// \todo Fix reconfigure process.
void reconfigure(context& ctx, const config& cfg, const cluster_config& cluster_cfg)
{
    ctx.election_distribution = std::uniform_int_distribution<size_t>(cfg.vote_timeout_min_ms, cfg.vote_timeout_max_ms);

    ctx.heartbeat_interval_ms = cfg.heartbeat_interval_ms;

    details::peers::update(ctx, cluster_cfg);
}

} // namespace utils

} // namespace details
} // namespace raft
} // namespace wstux
