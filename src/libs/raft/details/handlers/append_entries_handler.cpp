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

#include "raft/details/logger.h"
#include "raft/details/connection/send.h"
#include "raft/details/handlers/append_entries_handler.h"
#include "raft/details/handlers/snapshot_handler.h"
#include "raft/details/handlers/timeout_handler.h"
#include "raft/details/replication/entries.h"
#include "raft/details/role/convert.h"

namespace wstux {
namespace raft {
namespace details {
namespace append_entries {
namespace {

void handle_request_async(context& ctx, server_id_t src_id, bool accept, replication::entries::async::append_context::ptr p_async_ctx)
{
    assert(ctx.state.tasks_in_process > 0);

    --ctx.state.tasks_in_process;

    const term_t term = p_async_ctx->term;
    const index_t index = p_async_ctx->index;
    const index_t leader_commit = p_async_ctx->leader_commit;

    // Apply changes to the logical state of entry replication
    accept = replication::entries::append_callback(ctx, accept, term, index, leader_commit, p_async_ctx->entries);

    // Calculate the index of the last successfully stored entry
    const index_t last_log_index = accept ? p_async_ctx->last_stored : p_async_ctx->last_index;
    return utils::send<message_type::append_entries_response>(ctx, src_id, p_async_ctx->term, ctx.id, accept, last_log_index);
}

} // <anonymous> namespace

void handle_request(context& ctx, term_t term, server_id_t src_id, const append_entries_message& msg)
{
    RAFT_AE_LOG_TRACE(ctx, "Handle append entries. Request from server %llu to server %llu(%s), current term %u",
        src_id, ctx.id, ctx.role.str(), ctx.term);

    // Raft Paper, Section 5.1: If RPC request or response contains term T > currentTerm: set currentTerm = T
    role::update_term(ctx, term);
    // Raft Paper, Section 5.1: AppendEntries RPC: 1. Reply false if term < currentTerm
    if (ctx.term > term) {
        RAFT_AE_LOG_DEBUG(ctx, "Handle append entries. Local term %u is higher then request term %u", ctx.term, term);
        return utils::send<message_type::append_entries_response>(ctx, src_id, ctx.term, ctx.id, false, ctx.log.last_index());
    }

    assert(ctx.role.is_follower() || ctx.role.is_candidate());
    assert(ctx.term == term);

    // If we were a candidate and a valid request arrives from a leader with
    // term >= ctx.term, recognize it and step down
    // Raft Paper, Section 5.2: "While waiting for votes, a candidate may receive
    // an AppendEntries RPC... If the leader’s term... is at least as large as
    // the candidate’s current term, the candidate recognizes the leader as
    // legitimate and returns to follower state."
    if (ctx.role.is_candidate()) {
        role::become_follower(ctx);
    }

    assert(ctx.role.is_follower());

    // Update current leader because the term in this message is up to date.
    role::update_leader(ctx, src_id);
    timeout::election_restart_task(ctx);

    replication::entries::async::append_context::ptr p_async_ctx;
    // Invoke internal consistency check logic: the log must contain an entry at
    // msg.prev_log_index matching msg.prev_log_term
    // Raft Paper, Section 5.3: AppendEntries RPC: 2. Reply false if log doesn’t
    // contain entry at prevLogIndex matching prevLogTerm
    const bool accept = replication::entries::append(ctx, term, msg.leader_commit, msg.prev_log_index, msg.prev_log_term, msg.entries, p_async_ctx);
    // Support for asynchronous I/O
    if (accept && ctx.is_async_io && p_async_ctx) {
        scheduler::handler_type handler_fn = [&ctx, src_id, p_async_ctx = std::move(p_async_ctx)] () -> void {
            RAFT_AE_LOG_TRACE(ctx, "Server %llu(%s) is saving %zu entries to io storage asynchronously.",
                ctx.id, ctx.role.str(), p_async_ctx->entries.size());
            // Perform blocking disk write outside the critical section (mutex)
            const bool accept = ctx.p_io->append(p_async_ctx->entries);
            ctx.schd.execute_strand([&ctx, src_id, accept, p_async_ctx = std::move(p_async_ctx)] () -> void {
                handle_request_async(ctx, src_id, accept, p_async_ctx);
            });
        };
        ++ctx.state.tasks_in_process;
        ctx.schd.execute_async(std::move(handler_fn));
        return;
    }

    const index_t last_log_index = accept ? ctx.state.last_stored : ctx.log.last_index();
    return utils::send<message_type::append_entries_response>(ctx, src_id, ctx.term, ctx.id, accept, last_log_index);
}

void handle_response(context& ctx, term_t term, server_id_t src_id, const append_entries_response_message& msg)
{
    RAFT_AE_LOG_TRACE(ctx, "Handle append entries response. Response from server %llu to server %llu(%s), current term %u",
        src_id, ctx.id, ctx.role.str(), ctx.term);

    // Stale response that arrived after we already lost leadership
    if (! ctx.role.is_leader()) {
        return;
    }

    // Outdated response from previous terms — ignore
    if (ctx.term > term) {
        RAFT_AE_LOG_DEBUG(ctx, "Handle append entries response. Local term %u is higher then request term %u", ctx.term, term);
        return;
    }

    // Raft Paper, Section 5.1: "If RPC request or response contains term T > currentTerm:
    // set currentTerm = T, convert to follower"
    if (ctx.term < term) {
        role::update_term(ctx, term);
        assert(ctx.role.is_follower());
        return;
    }

    assert(ctx.term == term);
    assert(ctx.role.is_leader());

    peer::ptr p_src_peer = peers::find(ctx, src_id);
    if (! p_src_peer) {
        RAFT_AE_LOG_DEBUG(ctx, "Got append entries response message from removed server %llu", src_id);
        return;
    }

    // Reset the node availability timeout
    p_src_peer->mark_recent_recv();

    // Raft Paper, Section 5.3: Rules for Servers - Leaders: "If AppendEntries
    // fails because of log inconsistency: decrement nextIndex and retry"
    if (! msg.accept) {
        // reject changes
        /// \todo Need to implement decrement.
        //    decrement(ctx, p_peer, last_log_index);
        //    request(ctx, p_peer);
        return;
    }

    // Race condition protection: the index from the response cannot exceed the
    // current size of log
    const index_t last_index = std::min(msg.last_log_index, ctx.log.last_index());

    // Raft Paper, Section 5.3: Rules for Servers - Leaders: "If successful:
    // update nextIndex and matchIndex for follower"
    if (! p_src_peer->update_progress(last_index)) {
        return;
    }

    // Check if commitIndex can be advanced forward
    replication::entries::update_commit_index(ctx, last_index);
    // Raft Paper, Section 5.3 (State Machine Application): Apply committed
    // entries to the State Machine
    replication::entries::commit(ctx);
}


void request(context& ctx)
{
    RAFT_AE_LOG_TRACE(ctx, "Request append entries. Server %llu(%s), current term %u", ctx.id, ctx.role.str(), ctx.term);

    assert(ctx.role.is_leader());

    for (const peer::list::value_type& p : ctx.peers) {
        request(ctx, p);
    }
}

void request(context& ctx, const peer& p)
{
    assert(ctx.role.is_leader());
    assert(p.id != ctx.id);

    RAFT_AE_LOG_TRACE(ctx, "Request append entries to server %llu. Server %llu(%s), current term %u",
        p.id, ctx.id, ctx.role.str(), ctx.term);

    index_t snapshot_index = ctx.log.snapshot.last_index;
    index_t next_index = p.next_index;
    index_t prev_index = ctx.log.last_index();
    term_t prev_term = ctx.log.last_term();

    assert(next_index >= 1);

    // If the node initializes from the very beginning of the log
    if (next_index == 1) {
        if (snapshot_index > 0) {
            // Raft Paper, Section 7: If we already have a snapshot and the log
            // is compacted, we must send a Snapshot RPC
            assert(ctx.log.last_index() > 0);
            if (p.recent_recv) {
                RAFT_AE_LOG_TRACE(ctx, "Sending snapshot request to server %u. Server %llu(%s), current term %u",
                    p.id, ctx.id, ctx.role.str(), ctx.term);

                return snapshot::request(ctx, p);
            }
        } else {
            // Base case of an empty cluster startup
            prev_index = 0;
            prev_term = 0;
        }
    } else {
        // Normal calculation according to the specification: prevLogIndex = nextIndex - 1
        prev_index = next_index - 1;
        prev_term = ctx.log.term(prev_index);
        // Raft Paper, Section 7: If the term for prev_index returns 0, it means
        // this entry is already inside a compacted snapshot.
        if (prev_term == 0) {
            assert(prev_index < snapshot_index);
            if (p.recent_recv) {
                RAFT_AE_LOG_TRACE(ctx, "Sending snapshot request to server %u. Server %llu(%s), current term %u",
                    p.id, ctx.id, ctx.role.str(), ctx.term);

                // The leader is forced to send an InstallSnapshot RPC instead of AppendEntries
                return snapshot::request(ctx, p);
            }
        }
    }

    // Extract entries from the local log starting from prev_index + 1
    entry::list entries = ctx.log.acquire(prev_index + 1);

    RAFT_AE_LOG_TRACE(ctx, "Sending request to server %llu to append %zu entries with index %u. Server %llu(%s), current term %u",
        p.id, entries.size(), (prev_index + 1), ctx.id, ctx.role.str(), ctx.term);
    return utils::send<message_type::append_entries_request>(ctx, p.id, ctx.term, ctx.id,
        prev_index, prev_term, ctx.state.commit_index, std::move(entries));
}

} // namespace append_entries
} // namespace details
} // namespace raft
} // namespace wstux
