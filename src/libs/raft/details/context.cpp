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
#include "raft/details/logger.h"
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

    ctx.state.cluster_cfg = std::move(cluster_cfg);

    if (const server_config* p_cfg = utils::find_server_config(ctx, ctx.id)) {
        ctx.role.is_voter = p_cfg->is_voter;
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
    , p_io(p_io)
    , p_fsm(p_fsm)
    , term(0)
    , schd(alloc)
    , heartbeat_interval_ms(100)
    , rand_engine(std::chrono::system_clock::now().time_since_epoch().count() * id)
    , election_distribution(250, 500)
    , raft_logger(std::move(p_handler))
{
    state.snapshot.threshold = 512;
    state.snapshot.trailing = 1024;
}

namespace utils {

bool bootstrap(context& ctx, cluster_config cluster_cfg)
{
    ctx.state.cluster_cfg = std::move(cluster_cfg);
    std::sort(ctx.state.cluster_cfg.servers.begin(), ctx.state.cluster_cfg.servers.end(),
        [](const server_config& l, const server_config& r) -> bool { return l.id < r.id; });
    if (! utils::is_valid_cluster(ctx.id, ctx.state.cluster_cfg)) {
        return false;
    }
    //ctx.state.cluster_cfg = std::move(cluster_cfg);
    return true;
}

bool check_contact_quorum(context& ctx)
{
    assert(ctx.role.is_leader());

    size_t contacts = 1;
    size_t voting_count = 1;
    for (peer& p : ctx.role.leader.peers) {
        const bool recent_recv = p.reset_recent_recv();
        contacts += (p.is_voter && recent_recv) ? 1 : 0;
        voting_count += (p.is_voter) ? 1 : 0;
    }
    const size_t quorum_for_election_size = (voting_count / 2);
    return contacts > quorum_for_election_size;
}

peer::ptr find_peer(context& ctx, server_id_t id)
{
    assert(ctx.role.is_leader());

    peer::list::iterator it = std::find_if(ctx.role.leader.peers.begin(), ctx.role.leader.peers.end(),
        [id](const peer& p) { return p.id == id; });
    if (it != ctx.role.leader.peers.cend()) {
        return &(*it);
    }
    return peer::ptr();
}

server_config* find_server_config(context& ctx, server_id_t id)
{
    std::vector<server_config>::iterator it =
        std::find_if(ctx.state.cluster_cfg.servers.begin(), ctx.state.cluster_cfg.servers.end(),
            [id](const server_config& cfg) { return cfg.id == id; });
    if (it != ctx.state.cluster_cfg.servers.cend()) {
        return &(*it);
    }
    return nullptr;
}

bool init(context& ctx)
{
    if (! ctx.p_io->init(ctx.id)) {
        RAFT_LOG_ERROR(ctx, "Server %llu(%s) failed to init I/O.", ctx.id, ctx.role.str());
        return false;
    }

    const config cfg = ctx.p_io->configuration();
    if (cfg.heartbeat_interval_ms == 0 || cfg.vote_timeout_max_ms == 0 || cfg.vote_timeout_max_ms < cfg.vote_timeout_min_ms) {
        return false;
    }

    /*std::sort(cluster_cfg.servers.begin(), cluster_cfg.servers.end(),
        [](const server_config& l, const server_config& r) -> bool { return l.id < r.id; });
    if (! utils::is_valid_cluster(ctx.id, cluster_cfg)) {
        return false;
    }*/
    if (! ctx.state.cluster_cfg.servers.empty() && ! utils::is_valid_cluster(ctx.id, ctx.state.cluster_cfg)) {
        return false;
    }

    //ctx.state.cluster_cfg = std::move(cluster_cfg);

    ctx.is_async_io = cfg.is_async_io;

    ctx.role.voted_for = gk_invalid_id;

    ctx.schd.init(cfg.scheduler_threads_count);

    ctx.election_distribution = std::uniform_int_distribution<size_t>(cfg.vote_timeout_min_ms, cfg.vote_timeout_max_ms);
    ctx.heartbeat_interval_ms = cfg.heartbeat_interval_ms;

    ctx.state.snapshot.threshold = cfg.snapshot_threshold;
    ctx.state.snapshot.trailing = cfg.snapshot_trailing;

    ctx.raft_logger.is_heartbeat_channel_enabled = cfg.is_heartbeat_log_ch_enabled;
    ctx.raft_logger.is_snapshot_channel_enabled = cfg.is_snapshot_log_ch_enabled;
    ctx.raft_logger.is_timeout_channel_enabled = cfg.is_timeout_log_ch_enabled;
    ctx.raft_logger.is_vote_channel_enabled = cfg.is_vote_log_ch_enabled;

    ctx.state.commit_index = 0;
    ctx.state.last_applied = 0;
    ctx.state.last_stored = 0;
    ctx.state.tasks_in_process = 0;

    return true;
}

bool is_in_cluster(const context& ctx, server_id_t id)
{
    //if (ctx.id == id) {
    //    return false;
    //}
    std::vector<server_config>::const_iterator it =
        std::find_if(ctx.state.cluster_cfg.servers.cbegin(), ctx.state.cluster_cfg.servers.cend(),
            [id](const server_config& cfg) { return cfg.id == id; });
    return (it != ctx.state.cluster_cfg.servers.cend());
}

bool is_installing_snapshot(const context& ctx)
{
    return ctx.state.snapshot.is_in_process && (ctx.state.last_stored == 0);
}

bool is_valid_cluster(const server_id_t id, const cluster_config& cluster_cfg, bool check_self)
{
    assert(std::is_sorted(cluster_cfg.servers.cbegin(), cluster_cfg.servers.cend(),
        [](const server_config& l, const server_config& r) -> bool { return l.id < r.id; }));

    std::vector<server_config>::const_iterator it =
        std::adjacent_find(cluster_cfg.servers.cbegin(), cluster_cfg.servers.cend(),
            [](const server_config& l, const server_config& r) { return l.id == r.id; });
    if (it != cluster_cfg.servers.cend()) {
        return false;
    }
    if (check_self) {
        it = std::find_if(cluster_cfg.servers.cbegin(), cluster_cfg.servers.cend(),
            [id](const server_config& cfg) -> bool { return cfg.id == id; });
        return it != cluster_cfg.servers.cend();
    }
    return true;
}

bool load(context& ctx)
{
    io::ptr p_io = ctx.p_io;

    ctx.term = p_io->load_term();
    if (ctx.term == 0) {
        RAFT_LOG_ERROR(ctx, "Server %llu(%s) loaded invalid term (%u).", ctx.id, ctx.role.str(), ctx.term);
        return false;
    }
    ctx.role.voted_for = p_io->voted_for();

    index_t snapshot_index = p_io->load_snapshot_index();
    term_t snapshot_term = p_io->load_snapshot_term();
    index_t start_index = p_io->load_start_index();

    std::optional<snapshot> p_sh = p_io->get_snapshot();
    entry::list entries = p_io->load_entries();
    if (p_sh.has_value()) {
        if (! replication::snapshot::restore(ctx, *p_sh)) {
            RAFT_LOG_ERROR(ctx, "Server %llu(%s) failed to restore snapshot.", ctx.id, ctx.role.str());
            return false;
        }
        snapshot_index = p_sh->index;
        snapshot_term = p_sh->term;
    } else if (entries.size() > 0) {
        assert(start_index == 1);
        assert(entries[0]->type == entry_type::change);

        ctx.state.commit_index = 1;
        ctx.state.last_applied = 1;
    } else if (! ctx.state.cluster_cfg.servers.empty()) {
        entries.resize(1);
        entries[0] = std::make_shared<entry>();
        entry::ptr& e = entries[0];
        e->term = ctx.term;
        e->type = entry_type::change;
        e->buffer = serialize<cluster_config>(ctx.state.cluster_cfg);
    }

    if (! restore_entries(ctx, snapshot_index, snapshot_term, start_index, entries)) {
        RAFT_LOG_ERROR(ctx, "Server %llu(%s) failed to restore entries.", ctx.id, ctx.role.str());
        return false;
    }
    return true;
}

size_t quorum_for_election(const context& ctx)
{
    const size_t members_count = voting_members_count(ctx);
    return (members_count / 2);
}

/// \todo Fix reconfigure process.
void reconfigure(context& ctx, const config& cfg)
{
    ctx.is_async_io = cfg.is_async_io;

    ctx.schd.reconfigure(cfg.scheduler_threads_count);

    ctx.election_distribution = std::uniform_int_distribution<size_t>(cfg.vote_timeout_min_ms, cfg.vote_timeout_max_ms);
    ctx.heartbeat_interval_ms = cfg.heartbeat_interval_ms;

    ctx.state.snapshot.threshold = cfg.snapshot_threshold;
    ctx.state.snapshot.trailing = cfg.snapshot_trailing;

    ctx.raft_logger.is_heartbeat_channel_enabled = cfg.is_heartbeat_log_ch_enabled;
    ctx.raft_logger.is_snapshot_channel_enabled = cfg.is_snapshot_log_ch_enabled;
    ctx.raft_logger.is_timeout_channel_enabled = cfg.is_timeout_log_ch_enabled;
    ctx.raft_logger.is_vote_channel_enabled = cfg.is_vote_log_ch_enabled;
}

size_t voting_members_count(const context& ctx)
{
    assert(ctx.role.is_voter);
    return std::count_if(ctx.state.cluster_cfg.servers.cbegin(), ctx.state.cluster_cfg.servers.cend(),
        [](const server_config& cfg) -> bool { return cfg.is_voter; });
}

} // namespace utils
} // namespace details
} // namespace raft
} // namespace wstux
