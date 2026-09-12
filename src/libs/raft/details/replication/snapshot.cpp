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

#include "raft/details/logger.h"
#include "raft/details/connection/serialization.h"
#include "raft/details/replication/entries.h"
#include "raft/details/replication/snapshot.h"

namespace wstux {
namespace raft {
namespace details {
namespace replication {
namespace snapshot {
namespace {

void async_take_snapshot_cb(context& ctx, bool accept, raft::snapshot&& sh)
{
    ctx.state.snapshot.is_in_process = false;
    if (! accept) {
        RAFT_LOG_ERROR(ctx, "Server %llu(%s) failed to take snapshot.", ctx.id, ctx.role.str());
        return;
    }

    ctx.state.snapshot.cluster_cfg = std::move(sh.conf);
    ctx.log.take_snapshot(sh.index, ctx.state.snapshot.trailing);
}

void fetch_last_committed_configuration(const context& ctx, cluster_config& cluster_cfg)
{
    entry::ptr p_entry = ctx.log.get_entry(ctx.state.configuration_committed_index);
    if (p_entry) {
        deserialize(p_entry->buffer, cluster_cfg);
    } else {
        assert(ctx.state.snapshot.cluster_cfg.servers.size() > 0);
        cluster_cfg = ctx.state.snapshot.cluster_cfg;
    }
}

} // <anonymous> namespace

bool install(context& ctx, index_t last_index, term_t last_term, cluster_config conf,
             index_t conf_index, buffer_type buffer, async::install_context::ptr& p_async_ctx)
{
    assert(ctx.role.is_follower());
    assert(p_async_ctx.get() == nullptr);

    if (ctx.state.snapshot.is_in_process) {
        return false;
    }

    /* If our last snapshot is more up-to-date, this is a no-op */
    if (ctx.log.snapshot.last_index >= last_index) {
        RAFT_LOG_WARN(ctx, "Server %llu(%s) have more recent snapshot.", ctx.id, ctx.role.str());
        return true;
    }

    /* If we already have all entries in the snapshot, this is a no-op */
    const term_t local_term = ctx.log.term(last_index);
    if (local_term != 0 && local_term >= last_term) {
        RAFT_LOG_DEBUG(ctx, "Server %llu(%s) have all entries. State: commit_index %u, "
            "configuration_committed_index %u, configuration_uncommitted_index %u, last_applied %u, "
            "last_stored %u", ctx.id, ctx.role.str(), ctx.state.commit_index, ctx.state.configuration_committed_index,
            ctx.state.configuration_uncommitted_index, ctx.state.last_applied.load(std::memory_order_acquire), ctx.state.last_stored);
        return true;
    }

    ctx.log.restore(last_index, last_term);
    ctx.state.last_stored = 0;

    raft::snapshot sh;
    sh.index = last_index;
    sh.term = last_term;
    sh.conf = std::move(conf);
    sh.conf_index = conf_index;
    sh.buffer.swap(buffer);

    if (ctx.is_async_io) {
        p_async_ctx = std::make_shared<async::install_context>();
        p_async_ctx->snapshot = std::move(sh);
        p_async_ctx->last_log_index = ctx.state.last_stored;
        p_async_ctx->term = ctx.term;
        return true;
    }

    const bool accept = ctx.p_io->set_snapshot(sh);
    return install_callback(ctx, accept, sh);
}

bool install_callback(context& ctx, bool accept, raft::snapshot& snapshot)
{
    if (! accept) {
        return false;
    }
    if (! restore(ctx, snapshot)) {
        return false;
    }
    RAFT_LOG_DEBUG(ctx, "Server %llu(%s) installed snapshot. State: commit_index %u, "
            "configuration_committed_index %u, configuration_uncommitted_index %u, last_applied %u, "
            "last_stored %u", ctx.id, ctx.role.str(), ctx.state.commit_index, ctx.state.configuration_committed_index,
            ctx.state.configuration_uncommitted_index, ctx.state.last_applied.load(std::memory_order_acquire), ctx.state.last_stored);
    return true;
}

bool restore(context& ctx, raft::snapshot& snapshot)
{
    if (! ctx.p_fsm->restore(snapshot.buffer)) {
        RAFT_LOG_ERROR(ctx, "Server %llu(%s) failed to restore fsm state from snapshot %u.", ctx.id, ctx.role.str(), snapshot.index);
        return false;
    }

    //entries::apply_configuration(ctx, std::move(snapshot.conf));
    peers::update(ctx, snapshot.conf);
    ctx.state.configuration_committed_index = snapshot.conf_index;
    ctx.state.configuration_uncommitted_index = 0;

    ctx.state.snapshot.cluster_cfg = snapshot.conf;

    ctx.state.commit_index = snapshot.index;
    ctx.state.last_applied = snapshot.index;
    ctx.state.last_stored = snapshot.index;

    RAFT_LOG_DEBUG(ctx, "Server %llu(%s) restored snapshot. State: commit_index %u, "
            "configuration_committed_index %u, configuration_uncommitted_index %u, last_applied %u, "
            "last_stored %u", ctx.id, ctx.role.str(), ctx.state.commit_index, ctx.state.configuration_committed_index,
            ctx.state.configuration_uncommitted_index, ctx.state.last_applied.load(std::memory_order_acquire), ctx.state.last_stored);
    return true;
}

bool should_take_snapshot(context& ctx)
{
    if (ctx.state.snapshot.is_in_process) {
        return false;
    };

    if ((ctx.state.last_applied - ctx.log.snapshot.last_index) < ctx.state.snapshot.threshold) {
        return false;
    }

    return true;
}

bool take_snapshot(context& ctx)
{
    RAFT_LOG_DEBUG(ctx, "Server %llu(%s) takes snapshot at %u.", ctx.id, ctx.role.str(), ctx.state.last_applied.load(std::memory_order_acquire));

    raft::snapshot sh;
    sh.index = ctx.state.last_applied;
    sh.term = ctx.log.term(ctx.state.last_applied);

    fetch_last_committed_configuration(ctx, sh.conf);
    sh.conf_index = ctx.state.configuration_committed_index;

    if (! ctx.p_fsm->take_snapshot(sh.buffer)) {
        return false;
    }

    if (ctx.is_async_io) {
        scheduler::handler_type handler_fn = [&ctx, sh = std::move(sh)] () -> void {
            RAFT_LOG_TRACE(ctx, "Server %llu(%s) is taking snapshot asynchronously.", ctx.id, ctx.role.str());
            const bool accept = ctx.p_io->set_snapshot(sh);
            ctx.schd.execute_strand([&ctx, accept, sh = std::move(sh)] () mutable -> void {
                async_take_snapshot_cb(ctx, accept, std::move(sh));
            });
        };
        return true;
    }

    bool accept = ctx.p_io->set_snapshot(sh);
    async_take_snapshot_cb(ctx, accept, std::move(sh));
    return accept;
}

} // namespace snapshot
} // namespace replication
} // namespace details
} // namespace raft
} // namespace wstux
