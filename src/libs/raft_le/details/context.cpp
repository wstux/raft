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

bool load_peers(context& ctx, const cluster_config_t& cluster_cfg)
{
    bool is_loaded = false;
    for (const server_config& cfg : cluster_cfg) {
        if (ctx.id == cfg.id) {
            is_loaded = true;
            ctx.config = cfg;
            //ctx.role.is_voter = cfg.is_voter;
        } else if (peers::find(ctx, cfg.id).get() == nullptr) {
            peers::emplace(ctx, cfg);
        } else {
            return false;
        }
    }
    return is_loaded;
}

} // <anonymous> namespace

////////////////////////////////////////////////////////////////////////////////
// class context

context::context(server_id_t id, const io::ptr p_io, const ilogger_factory::ptr p_factory, const is_stop_fn_t& is_stop)
    : id(id)
    , is_stop_fn(is_stop)
    , is_async_io(false)
    , p_io(p_io)
    , term(0)
    , heartbeat_interval_ms(100)
    , heartbeat_probes_count(10)
    , rand_engine(std::chrono::system_clock::now().time_since_epoch().count() * id)
    , election_distribution(250, 500)
    , l(p_factory)
{}

std::ostream& operator<<(std::ostream& os, const context& ctx)
{
    os << ctx.id << "(" << ctx.role.str() << ")";
    return os;
}

namespace peers {

bool emplace(context& ctx, const server_config& cfg)
{
    if (ctx.id == cfg.id) {
        return false;
    }

    const size_t hb_expired_interval_ms = ctx.heartbeat_interval_ms * ctx.heartbeat_probes_count;
    peer::ptr p_peer = std::make_shared<peer>(cfg, ctx.p_io, hb_expired_interval_ms);

    std::unique_lock<std::shared_mutex> lock(ctx.peers_mutex);
    ctx.peers.emplace(cfg.id, p_peer);
    return true;
}

peer::ptr find(context& ctx, server_id_t id)
{
    std::shared_lock<std::shared_mutex> lock(ctx.peers_mutex);
    peer::map::const_iterator it = ctx.peers.find(id);
    if (it != ctx.peers.cend()) {
        return it->second;
    }
    return peer::ptr();
}

size_t quorum_for_election(context& ctx)
{
    const size_t members_count = voting_members_count(ctx);
    return (members_count / 2);
}

size_t size(context& ctx)
{
    std::shared_lock<std::shared_mutex> lock(ctx.peers_mutex);
    return ctx.peers.size();
}

void swap(context& ctx, peer::map& peers)
{
    std::unique_lock<std::shared_mutex> lock(ctx.peers_mutex);
    ctx.peers.swap(peers);
}

peer::list to_list(context& ctx)
{
    peer::list peers;
    {
        std::shared_lock<std::shared_mutex> lock(ctx.peers_mutex);
        peers.reserve(ctx.peers.size());
        std::transform(ctx.peers.begin(), ctx.peers.end(), std::back_inserter(peers),
            [](const peer::map::value_type& p) -> peer::ptr { return p.second; });
    }
    return peers;
}

size_t voting_members_count(context& ctx)
{
    using peer_value = peer::map::value_type;

    //assert(ctx.role.is_voter);

    std::shared_lock<std::shared_mutex> lock(ctx.peers_mutex);
    return 1 + std::count_if(ctx.peers.cbegin(), ctx.peers.cend(),
                             [](const peer_value& v) -> bool { return v.second->is_voter(); });
}

} // namespace peers

namespace sturtup {

bool init(context& ctx, server_id_t id)
{
    assert(ctx.id == id);

    if (! ctx.p_io->init(id)) {
        return false;
    }

    const config& cfg = ctx.p_io->configuration();
    if (cfg.heartbeat_interval_ms == 0 || cfg.vote_timeout_max_ms == 0 || cfg.vote_timeout_max_ms < cfg.vote_timeout_min_ms) {
        return false;
    }

    ctx.role.voted_for = gk_invalid_id;

    ctx.p_scheduler = std::make_shared<scheduler>(cfg.scheduler_threads_count);

    ctx.election_distribution = std::uniform_int_distribution<size_t>(cfg.vote_timeout_min_ms, cfg.vote_timeout_max_ms);
    ctx.heartbeat_interval_ms = cfg.heartbeat_interval_ms;
    ctx.heartbeat_probes_count = cfg.heartbeat_probes_count;

    return true;
}

bool load(context& ctx)
{
    io::ptr p_io = ctx.p_io;
    if (! p_io->load()) {
        return false;
    }

    ctx.term = p_io->load_term();

    const cluster_config_t& cluster_cfg = p_io->bootstrap();
    if (! load_peers(ctx, cluster_cfg)) {
        return false;
    }
    return true;
}

} // namespace sturtup

namespace utils {

size_t current_time_ms()
{
    using clock_t = std::chrono::steady_clock;
    using time_point_t = std::chrono::time_point<clock_t>;

    time_point_t cur = clock_t::now();
    std::chrono::duration<size_t, std::milli> cur_ms = std::chrono::duration_cast<std::chrono::milliseconds>(cur.time_since_epoch());
    return static_cast<std::size_t>(cur_ms.count());
}

cluster_config_t make_config(context& ctx)
{
    const peer::list peers = peers::to_list(ctx);

    cluster_config_t cfg;
    cfg.reserve(peers.size() + 1);

    cfg.emplace_back(ctx.config);
    for (const peer::list::value_type& p : peers) {
        cfg.emplace_back(p->config());
    }

    return cfg;
}

} // namespace utils

} // namespace details
} // namespace le
} // namespace raft
} // namespace wstux
