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
#include <limits>

#include "raft/details/logger.h"
#include "raft/details/replication/entries.h"
#include "raft/details/replication/membership.h"
#include "raft/details/replication/snapshot.h"
#include "raft/details/role/convert.h"

namespace wstux {
namespace raft {
namespace details {
namespace replication {
namespace entries {
namespace {

void commmit_change(context& ctx, const index_t index)
{
    assert(index > 0);

    if (ctx.state.configuration_uncommitted_index == index) {
        ctx.state.configuration_uncommitted_index = 0;
    }

    ctx.state.configuration_committed_index = index;
    ctx.state.last_applied = index;

    if (ctx.role.is_leader()) {
        if (! ctx.role.is_voter) {
            role::become_follower(ctx);
        }
    }
}

bool commmit_command(context& ctx, const index_t index, const entry::ptr& p_entry)
{
    if (! ctx.p_fsm->apply(p_entry->buffer)) {
        RAFT_LOG_WARN(ctx, "Failed to commit command %u to fsm.", index);
        return false;
    }

    RAFT_LOG_DEBUG(ctx, "Committed command %u to fsm.", index);
    ctx.state.last_applied = index;
    return true;
}

bool is_consistent_log(context& ctx, index_t prev_log_index, term_t prev_log_term)
{
    if (prev_log_index == 0 || ctx.log.entries.empty()) {
        return true;
    }

    const term_t local_prev_term = ctx.log.term(prev_log_index);
    if (local_prev_term == 0) {
        return false;
    }

    if (local_prev_term != prev_log_term) {
        assert(prev_log_index <= ctx.state.commit_index);
        return false;
    }

    return true;
}

size_t resolve_conflicts(context& ctx, index_t prev_log_index, const entry::list& entries)
{
    for (size_t i = 0; i < entries.size(); ++i) {
        entry::ptr p_entry = entries[i];
        index_t entry_index = prev_log_index + 1 + i;
        term_t local_term = ctx.log.term(entry_index);

        if (local_term > 0 && local_term != p_entry->term) {
            assert(entry_index <= ctx.state.commit_index);
            if (ctx.state.configuration_uncommitted_index >= entry_index) {
                return 0;
            }
            // Delete all entries from this index on because they don't match.
            if (! ctx.p_io->truncate(entry_index)) {
                return std::numeric_limits<size_t>::max();
            }
            ctx.log.truncate(entry_index);

            if (ctx.state.last_stored >= entry_index) {
                ctx.state.last_stored = entry_index - 1;
            }

            return i;
        } else if (local_term == 0) {
            return i;
        }
    }
    return entries.size();
}

bool store_log_to_storage(context& ctx, index_t index, async::apply_context::ptr& p_async_ctx)
{
    entry::list entries = ctx.log.acquire(index);
    assert(entries.size() > 0);

    if (ctx.is_async_io) {
        p_async_ctx = std::allocate_shared<async::apply_context>(ctx.alloc);
        p_async_ctx->index = index;
        p_async_ctx->entries.swap(entries);
        return true;
    }

    const bool accept = ctx.p_io->append(entries);
    return apply_callback(ctx, accept, index, entries);
}

size_t update_last_stored(context& ctx, index_t first_index, const entry::list& entries)
{
    size_t i = 0;
    for (i = 0; i < entries.size(); ++i) {
        const entry::ptr& p_entry = entries[i];
        index_t index = first_index + i;
        term_t local_term = ctx.log.term(index);

        if (local_term == 0 || (local_term > 0 && local_term != p_entry->term)) {
            break;
        }
        assert(local_term != 0 && local_term == p_entry->term);
    }
    ctx.state.last_stored += i;
    return i;
}

bool update_configuration(context& ctx, index_t first_index, term_t term, index_t leader_commit, const entry::list& entries)
{
    const size_t i = update_last_stored(ctx, first_index, entries);
    if (i == 0) {
        return false;
    }

    entry::ptr p_config_entry;
    for (size_t j = 0; j < i; j++) {
        entry::ptr p_entry = entries[j];
        [[maybe_unused]] index_t index = first_index + j;
        [[maybe_unused]] term_t local_term = ctx.log.term(index);

        assert(local_term != 0 && local_term == p_entry->term);

        if (p_entry->type == entry_type::change) {
            p_config_entry = p_entry;
        }
    }

    if (p_config_entry) {
        if (! membership::update(ctx, p_config_entry)) {
            return false;
        }
    }

    if (leader_commit > ctx.state.commit_index && ctx.state.last_stored >= ctx.state.commit_index) {
        ctx.state.commit_index = std::min(leader_commit, ctx.state.last_stored);
        if (! commit(ctx)) {
            return false;
        }
    }

    if (ctx.term != term) {
        return false;
    }

    return true;
}

} // <anonymous> namespace

bool append(context& ctx, term_t term, index_t leader_commit, index_t prev_log_index, term_t prev_log_term,
            const entry::list& entries, async::append_context::ptr& p_async_ctx)
{
    RAFT_LOG_TRACE(ctx, "Server %llu(%s) is adding new %zu entries.", ctx.id, ctx.role.str(), entries.size());

    assert(ctx.role.is_follower());
    assert(p_async_ctx.get() == nullptr);

    if (! is_consistent_log(ctx, prev_log_index, prev_log_term)) {
        return false;
    }

    /* Delete conflicting entries. */
    const size_t begin = resolve_conflicts(ctx, prev_log_index, entries);
    if (begin == std::numeric_limits<size_t>::max()) {
        RAFT_LOG_WARN(ctx, "Server %llu(%s) failed to resolve conflicts.", ctx.id, ctx.role.str());
        return false;
    }

    RAFT_LOG_TRACE(ctx, "Server %llu(%s) got %zu entries with leader_commit %u and previous log index %u.",
        ctx.id, ctx.role.str(), entries.size(), leader_commit, prev_log_index);
    if (begin == entries.size()) {
        RAFT_LOG_TRACE(ctx, "Server %llu(%s) does not need to add new entries.", ctx.id, ctx.role.str());
        if ((leader_commit > ctx.state.commit_index) && ctx.state.last_stored >= ctx.state.commit_index) {
            ctx.state.commit_index = std::min(leader_commit, ctx.state.last_stored);
            RAFT_LOG_TRACE(ctx, "Server %llu(%s) updated commit index. State: commit_index %u, "
                "configuration_committed_index %u, configuration_uncommitted_index %u, last_applied %u, "
                "last_stored %u", ctx.id, ctx.role.str(), ctx.state.commit_index, ctx.state.configuration_committed_index,
                ctx.state.configuration_uncommitted_index, ctx.state.last_applied, ctx.state.last_stored);
            if (! commit(ctx)) {
                return false;
            }
        }
        return true;
    }

    for (size_t i = begin; i < entries.size(); ++i) {
        entry::ptr p_entry = entries[i];
        ctx.log.append(p_entry);
    }
    RAFT_LOG_TRACE(ctx, "Server %llu(%s) added %zu entries.", ctx.id, ctx.role.str(), (entries.size() - begin));

    const index_t index = prev_log_index + begin + 1;
    entry::list ac_entries = ctx.log.acquire(index);

    assert(ac_entries.size() != 0);

    if (ctx.is_async_io) {
        p_async_ctx = std::allocate_shared<async::append_context>(ctx.alloc);
        p_async_ctx->term = term;
        p_async_ctx->index = index;
        p_async_ctx->leader_commit = leader_commit;
        p_async_ctx->last_stored = ctx.state.last_stored;
        p_async_ctx->last_index = ctx.log.last_index();
        p_async_ctx->entries.swap(ac_entries);
        return true;
    }

    RAFT_LOG_TRACE(ctx, "Server %llu(%s) is saving %zu entries to io storeage.", ctx.id, ctx.role.str(), ac_entries.size());
    const bool accept = ctx.p_io->append(ac_entries);
    return append_callback(ctx, accept, term, index, leader_commit, entries);
}

bool append_callback(context& ctx, bool accept, term_t term, index_t index, index_t leader_commit, const entry::list& entries)
{
    if (! accept) {
        ctx.log.truncate(index);
        return false;
    }

    if (! update_configuration(ctx, index, term, leader_commit, entries)) {
        return false;
    }

    RAFT_LOG_TRACE(ctx, "Server %llu(%s) added %zu entries.", ctx.id, ctx.role.str(), entries.size());
    return true;
}

bool apply_command(context& ctx, buffer_type buf, async::apply_context::ptr& p_async_ctx)
{
    if (! ctx.role.is_leader()) {
        return false;
    }

    if (buf.size() == 0) {
        return false;
    }

    RAFT_LOG_TRACE(ctx, "Server %llu(%s) is applying new command.", ctx.id, ctx.role.str());

    /* Index of the first entry being appended. */
    index_t index = ctx.log.last_index() + 1;

    ctx.log.append_command(ctx.term, std::move(buf));
    assert(index == ctx.log.last_index());
    return store_log_to_storage(ctx, index, p_async_ctx);
}

bool apply_configuration(context& ctx, cluster_config cluster_cfg, async::apply_context::ptr& p_async_ctx)
{
    if (! ctx.role.is_leader()) {
        return false;
    }

    RAFT_LOG_TRACE(ctx, "Server %llu(%s) is applying new configuration.", ctx.id, ctx.role.str());

    const index_t index = ctx.log.last_index() + 1;

    ctx.log.append_change(ctx.term, cluster_cfg);
    assert(index == ctx.log.last_index());
    return store_log_to_storage(ctx, index, p_async_ctx);
}

bool apply_callback(context& ctx, bool accept, index_t index, const entry::list& entries)
{
    //if (ctx.role.is_leader()) {
    //    return false;
    //}

    if (! accept) {
        RAFT_LOG_ERROR(ctx, "Server %llu(%s) failed to add new entries to persistent storage.", ctx.id, ctx.role.str());
        if (index <= ctx.log.last_index()) {
            ctx.log.truncate(index);
        }
        if (ctx.role.is_leader()) {
            role::become_follower(ctx);
        }
        return false;
    }

    RAFT_LOG_TRACE(ctx, "Server %llu(%s) stored %zu to persistent storage.", ctx.id, ctx.role.str(), entries.size());
    update_last_stored(ctx, index, entries);
    if (! ctx.role.is_leader()) {
        return false;
    }
    update_commit_index(ctx, index);
    commit(ctx);
    return true;
}

bool commit(context& ctx)
{
    RAFT_LOG_TRACE(ctx, "Server %llu(%s) is committing commands.", ctx.id, ctx.role.str());

    assert(ctx.role.is_leader() || ctx.role.is_follower());
    assert(ctx.state.last_applied <= ctx.state.commit_index);

    if (ctx.state.last_applied == ctx.state.commit_index) {
        return true;
    }

    for (index_t i = ctx.state.last_applied + 1; i <= ctx.state.commit_index; ++i) {
        entry::ptr p_entry = ctx.log.get_entry(i);
        if (! p_entry) {
            return true;
        }

        bool rc = false;
        switch (p_entry->type) {
            case entry_type::change:
                commmit_change(ctx, i);
                rc = true;
                break;
            case entry_type::command:
                rc = commmit_command(ctx, i, p_entry);
                break;
            default:
                break;
        }

        if (! rc) {
            return false;
        }
    }

    RAFT_LOG_TRACE(ctx, "Server %llu(%s) committed changes. State: commit_index %u, "
                "configuration_committed_index %u, configuration_uncommitted_index %u, last_applied %u, "
                "last_stored %u", ctx.id, ctx.role.str(), ctx.state.commit_index, ctx.state.configuration_committed_index,
                ctx.state.configuration_uncommitted_index, ctx.state.last_applied, ctx.state.last_stored);
    if (snapshot::should_take_snapshot(ctx)) {
        return snapshot::take_snapshot(ctx);
    }
    return true;
}

void update_commit_index(context& ctx, const index_t index)
{
    assert(ctx.role.is_leader());

    // The index cannot decrease
    if (index <= ctx.state.commit_index) {
        return;
    }

    const term_t term = ctx.log.term(index);
    if (term == 0) {
        return;
    }

    // Raft prohibits committing entries from previous terms by counting replicas.
    // A leader can only commit entries from previous terms indirectly by committing
    // an entry from its current term (5.4.2).
    // "Raft never commits log entries from previous terms by counting replicas."
    assert(ctx.term >= term);
    if (term < ctx.term) {
        return;
    }

    // Count votes: 1 (the leader itself) + the number of peers whose match_index >= index
    size_t votes = 1 + std::count_if(ctx.peers.begin(), ctx.peers.end(),
                                     [index](const peer& p) -> bool { return p.is_voter && (p.match_index >= index); });

    // Check if the cluster configuration quorum is reached
    if (votes > peers::quorum_for_election(ctx)) {
        ctx.state.commit_index = index;
        RAFT_LOG_TRACE(ctx, "Entries replication reached quorum. Commit index has been updated to %u.", ctx.state.commit_index);
    }
}

} // namespace entries
} // namespace replication
} // namespace details
} // namespace raft
} // namespace wstux
