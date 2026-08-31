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

#include <algorithm>

#include "raft/details/connection/serialization.h"
#include "raft/details/replication/membership.h"
#include "raft/details/role/convert.h"

namespace wstux {
namespace raft {
namespace details {
namespace replication {
namespace membership {

bool append(context& ctx, const server_config& cfg)
{
    if (peers::find(ctx, cfg.id) != nullptr) {
        RAFT_LOG_TRACE(ctx, "Adding new member to cluster. Server %llu already exists in cluster with leader %llu(%s).",
            cfg.id, ctx.id, ctx.role.str());
        return false;
    }

    //cluster_config cluster_cfg = ctx.make_config();
    //cluster_cfg.servers.push_back(cfg);

    //if (! entries::apply_configuration(ctx, std::move(cluster_cfg))) {
    //    return false;
    //}

    RAFT_LOG_TRACE(ctx, "Server %llu(%s) is adding new peer with id %llu.", ctx.id, ctx.role.str(), cfg.id);
    peers::emplace(ctx, cfg);
    return true;
}

bool remove(context& ctx, const server_id_t id)
{
    if (! peers::find(ctx, id)) {
        RAFT_LOG_TRACE(ctx, "Removing member from cluster. Server %llu already is not exist in cluster with leader %llu(%s).",
            id, ctx.id, ctx.role.str());
        return false;
    }

    RAFT_LOG_TRACE(ctx, "Server %llu(%s) is removing existing peer with id %llu.", ctx.id, ctx.role.str(), id);
    peers::remove(ctx, id);

    //cluster_config cluster_cfg = ctx.make_config();
    //return entries::apply_configuration(ctx, std::move(cluster_cfg));
    return true;
}

bool update(context& ctx, const entry::ptr& p_entry)
{
    if (p_entry->type != entry_type::change) {
        return false;
    }

    cluster_config cluster_cfg = deserialize<cluster_config>(p_entry->buffer);
    std::sort(cluster_cfg.servers.begin(), cluster_cfg.servers.end(),
        [](const server_config& l, const server_config& r) -> bool { return l.id < r.id; });

    std::vector<server_config>::const_iterator it =
        std::adjacent_find(cluster_cfg.servers.cbegin(), cluster_cfg.servers.cend(),
            [](const server_config& l, const server_config& r) { return l.id == r.id; });
    if (it != cluster_cfg.servers.cend()) {
        return false;
    }

    peers::update(ctx, cluster_cfg);

    it = std::find_if(cluster_cfg.servers.cbegin(), cluster_cfg.servers.cend(),
        [&ctx](const server_config& cfg) -> bool { return cfg.id == ctx.id; });
    if (it == cluster_cfg.servers.cend()) {
        if (! ctx.role.is_follower()) {
            role::become_follower(ctx);
        }
    }
    return true;
}

} // namespace membership
} // namespace replication
} // namespace details
} // namespace raft
} // namespace wstux
