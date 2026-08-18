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
#include <chrono>
#include <mutex>

#include "raft_le/details/context.h"

namespace wstux {
namespace raft {
namespace le {
namespace details {
namespace {

peer::ptr find_peer(peer::list& peers, server_id_t id)
{
    peer::list::iterator it = std::find_if(peers.begin(), peers.end(), [id](const peer& p) {
        return p.id() == id;
    });
    if (it != peers.cend()) {
        return &(*it);
    }
    return peer::ptr();
}

bool load_peers(context& ctx, peer::list& peers, const cluster_config& cluster_cfg)
{
    const server_config* p_cur_cfg = nullptr;
    peers.reserve(cluster_cfg.servers.size() - 1);
    for (const server_config& cfg : cluster_cfg.servers) {
        if (ctx.id == cfg.id) {
            p_cur_cfg = &cfg;
            ctx.config = cfg;
            ctx.role.is_voter = cfg.is_voter;
        } else if (find_peer(peers, cfg.id) == nullptr) {
            peers.emplace_back(cfg);
        } else {
            return false;
        }
    }
    if (p_cur_cfg == nullptr) {
        return false;
    }
    ctx.config = *p_cur_cfg;
    ctx.role.is_voter = ctx.config.is_voter;
    return true;
}

} // <anonymous> namespace

////////////////////////////////////////////////////////////////////////////////
// class context

context::context(server_id_t id, const io::ptr p_io, logging_handler::ptr p_handler, const is_stop_fn_t& is_stop)
    : id(id)
    , is_stop_fn(is_stop)
    , is_async_io(false)
    , config(gk_invalid_id, false)
    , p_io(p_io)
    , term(0)
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
        contacts += (p.is_voter() && recent_recv) ? 1 : 0;
        voting_count += (p.is_voter()) ? 1 : 0;
    }
    const size_t quorum_for_election_size = (voting_count / 2);
    return contacts > quorum_for_election_size;
}

peer::ptr find(context& ctx, server_id_t id)
{
    return find_peer(ctx.peers, id);
}

size_t quorum_for_election(context& ctx)
{
    const size_t members_count = voting_members_count(ctx);
    return (members_count / 2);
}

bool update(context& ctx, const cluster_config& cluster_cfg)
{
    peer::list peers;
    if (! load_peers(ctx, peers, cluster_cfg)) {
        return false;
    }

    ctx.peers.swap(peers);
    return true;
}

size_t voting_members_count(context& ctx)
{
    assert(ctx.role.is_voter);

    return 1 + std::count_if(ctx.peers.cbegin(), ctx.peers.cend(),
        [](const peer& p) -> bool { return p.is_voter(); });
}

} // namespace peers

namespace utils {

size_t current_time_ms()
{
    using clock_t = std::chrono::steady_clock;
    using time_point_t = std::chrono::time_point<clock_t>;

    time_point_t cur = clock_t::now();
    std::chrono::duration<size_t, std::milli> cur_ms = std::chrono::duration_cast<std::chrono::milliseconds>(cur.time_since_epoch());
    return static_cast<std::size_t>(cur_ms.count());
}

bool init(context& ctx, server_id_t id)
{
    assert(ctx.id == id);

    if (! ctx.p_io->init(id)) {
        return false;
    }

    const config cfg = ctx.p_io->configuration();
    if (cfg.heartbeat_interval_ms == 0 || cfg.vote_timeout_max_ms == 0 || cfg.vote_timeout_max_ms < cfg.vote_timeout_min_ms) {
        return false;
    }

    ctx.role.voted_for = gk_invalid_id;

    ctx.p_scheduler = std::make_shared<scheduler>(cfg.scheduler_threads_count);

    ctx.election_distribution = std::uniform_int_distribution<size_t>(cfg.vote_timeout_min_ms, cfg.vote_timeout_max_ms);
    ctx.heartbeat_interval_ms = cfg.heartbeat_interval_ms;

    return true;
}

bool load(context& ctx)
{
    if (! ctx.peers.empty()) {
        return false;
    }

    io::ptr p_io = ctx.p_io;

    ctx.term = p_io->load_term();
    ctx.role.voted_for = p_io->voted_for();
    const cluster_config cluster_cfg = p_io->bootstrap();
    if (! load_peers(ctx, ctx.peers, cluster_cfg)) {
        return false;
    }
    return true;
}

void reconfigure(context& ctx, const config& cfg, const cluster_config& cluster_cfg)
{
    ctx.election_distribution = std::uniform_int_distribution<size_t>(cfg.vote_timeout_min_ms, cfg.vote_timeout_max_ms);

    ctx.heartbeat_interval_ms = cfg.heartbeat_interval_ms;

    details::peers::update(ctx, cluster_cfg);
}

} // namespace utils

} // namespace details
} // namespace le
} // namespace raft
} // namespace wstux
