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

#include "raft/details/connection/serialization.h"
#include "raft/details/handlers/append_entries_handler.h"
#include "raft/details/replication/entries.h"
#include "raft/details/replication/membership.h"
#include "raft/details/role/convert.h"

namespace wstux {
namespace raft {
namespace details {
namespace replication {
namespace membership {
namespace {

bool change_configuration(context& ctx)
{
    const bool accept = entries::apply_configuration(ctx, ctx.state.cluster_cfg);
    if (accept) {
        ctx.state.configuration_uncommitted_index = ctx.log.last_index();
        append_entries::request(ctx);
    }
    return accept;
}

} // <anonymous> namespace

bool append(context& ctx, const server_config& cfg)
{
    if (! is_configuration_enabled(ctx)) {
        RAFT_LOG_TRACE(ctx, "Adding new member to cluster. Configuration does not enable for changing.");
        return false;
    }

    if (utils::is_in_cluster(ctx, cfg.id)) {
        RAFT_LOG_TRACE(ctx, "Adding new member to cluster. Server %llu already exists in cluster with leader %llu(%s).",
            cfg.id, ctx.id, ctx.role.str());
        return false;
    }

    //assert(ctx.state.configuration_committed_index > 0);
    assert(ctx.log.last_index() >= ctx.state.configuration_committed_index);

    RAFT_LOG_TRACE(ctx, "Server %llu(%s) is adding new peer with id %llu.", ctx.id, ctx.role.str(), cfg.id);
    assert(ctx.role.is_leader());
    ctx.state.cluster_cfg.servers.push_back(cfg);
    std::sort(ctx.state.cluster_cfg.servers.begin(), ctx.state.cluster_cfg.servers.end(),
        [](const server_config& l, const server_config& r) -> bool { return l.id < r.id; });

    ctx.role.leader.peers.emplace_back(cfg, 1);
    std::sort(ctx.role.leader.peers.begin(), ctx.role.leader.peers.end(), [](const peer& l, const peer& r) -> bool { return l.id < r.id; });

    assert(ctx.state.cluster_cfg.servers.size() == (ctx.role.leader.peers.size() + 1));
    return change_configuration(ctx);
}

bool apply(context& ctx, buffer_type buf)
{
    const bool accept = entries::apply_command(ctx, std::move(buf));
    if (accept) {
        append_entries::request(ctx);
    }
    return accept;
}

bool is_configuration_enabled(context& ctx)
{
    if (! ctx.role.is_leader()) {
        RAFT_LOG_TRACE(ctx, "Server %llu(%s) is not leader. Configuration is disabled.", ctx.id, ctx.role.str());
        return false;
    }

    if (ctx.state.configuration_uncommitted_index != 0) {
        RAFT_LOG_TRACE(ctx, "Server %llu(%s) has uncommitted configuration. Configuration is disabled.", ctx.id, ctx.role.str());
        return false;
    }

    //assert(ctx.state.configuration_committed_index > 0);
    assert(ctx.log.last_index() >= ctx.state.configuration_committed_index);

    return true;
}

bool remove(context& ctx, const server_id_t id)
{
    if (! is_configuration_enabled(ctx)) {
        RAFT_LOG_TRACE(ctx, "Removing member from cluster. Configuration does not enable for changing.");
        return false;
    }

    if (! utils::is_in_cluster(ctx, id)) {
        RAFT_LOG_TRACE(ctx, "Removing member from cluster. Server %llu already is not exist in cluster with leader %llu(%s).",
            id, ctx.id, ctx.role.str());
        return false;
    }

    RAFT_LOG_TRACE(ctx, "Server %llu(%s) is removing existing peer with id %llu.", ctx.id, ctx.role.str(), id);
    assert(ctx.role.is_leader());
    ctx.state.cluster_cfg.servers.erase(
        std::remove_if(ctx.state.cluster_cfg.servers.begin(), ctx.state.cluster_cfg.servers.end(),
            [id](const server_config& s) { return s.id == id; }),
        ctx.state.cluster_cfg.servers.end()
    );
    ctx.role.leader.peers.erase(
        std::remove_if(ctx.role.leader.peers.begin(), ctx.role.leader.peers.end(), [id](const peer& p) { return p.id == id; }),
        ctx.role.leader.peers.end()
    );

    assert(ctx.state.cluster_cfg.servers.size() == (ctx.role.leader.peers.size() + 1));
    return change_configuration(ctx);
}

bool update(context& ctx, const entry::ptr& p_entry)
{
    assert(! ctx.role.is_leader());

    if (p_entry->type != entry_type::change) {
        return false;
    }

    cluster_config cluster_cfg = deserialize<cluster_config>(p_entry->buffer);
    std::sort(cluster_cfg.servers.begin(), cluster_cfg.servers.end(),
        [](const server_config& l, const server_config& r) -> bool { return l.id < r.id; });

    if (! utils::is_valid_cluster(ctx.id, cluster_cfg, false)) {
        return false;
    }

    std::vector<server_config>::const_iterator it =
        std::find_if(cluster_cfg.servers.cbegin(), cluster_cfg.servers.cend(),
            [&ctx](const server_config& cfg) -> bool { return cfg.id == ctx.id; });
    if (it == cluster_cfg.servers.cend()) {
        return false;
        /*if (! ctx.role.is_follower()) {
            role::become_follower(ctx);
        }*/
    }

    //server::update(ctx, cluster_cfg);
    ctx.state.cluster_cfg = std::move(cluster_cfg);

    return true;
}

/*namespace server {

void update(context& ctx, cluster_config& cluster_cfg)
{
    ctx.state.cluster_cfg = std::move(cluster_cfg);

    if (! ctx.role.is_leader()) {
        return;
    }

    ctx.role.leader.peers.reserve(std::max(ctx.role.leader.peers.capacity(), ctx.state.cluster_cfg.servers.size()));

    std::vector<server_config>::const_iterator srv_it = ctx.state.cluster_cfg.servers.cbegin();
    ctx.role.leader.peers.erase(
        std::remove_if(ctx.role.leader.peers.begin(), ctx.role.leader.peers.end(),
            [&srv_it, &ctx](const peer& p) {
                srv_it = std::find_if(srv_it, ctx.state.cluster_cfg.servers.cend(),
                    [&p](const server_config& s) { return s.id >= p.id; });
                return (srv_it == ctx.state.cluster_cfg.servers.end() || srv_it->id != p.id);
            }
        ),
        ctx.role.leader.peers.end()
    );

    for (const server_config& cfg : ctx.state.cluster_cfg.servers) {
        if (ctx.id != cfg.id) {
            peer::ptr p_peer = utils::find_peer(ctx, cfg.id);
            if (p_peer == nullptr) {
                ctx.role.leader.peers.emplace_back(cfg, ctx.log.last_index() + 1);
            } else {
                p_peer->address = cfg.address;
                p_peer->is_voter = cfg.is_voter;
            }
        } else {
            ctx.role.is_voter = cfg.is_voter;
        }
    }
    std::sort(ctx.role.leader.peers.begin(), ctx.role.leader.peers.end(),
        [](const peer& l, const peer& r) -> bool { return l.id < r.id; });
}

}*/ // namespace server
} // namespace membership
} // namespace replication
} // namespace details
} // namespace raft
} // namespace wstux
