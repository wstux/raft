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
#include "raft/details/connection/send.h"
#include "raft/details/handlers/snapshot_handler.h"
#include "raft/details/handlers/timeout_handler.h"
#include "raft/details/replication/snapshot.h"
#include "raft/details/role/convert.h"

namespace wstux {
namespace raft {
namespace details {
namespace snapshot {
namespace {

/**
 *  \brief  Asynchronously applies a snapshot to the I/O subsystem and sends
 *      a response to the leader.
 *  \param  ctx - current server state context.
 *  \param  src_id - identifier of the leader that sent the snapshot.
 *  \param  p_async_ctx - context of the asynchronous installation, containing
 *      snapshot data and original request metadata.
 *
 *  \details    This method executes within a dedicated worker thread to avoid
 *      blocking the main event loop with heavy disk operations.
 */
void handle_request_async(context& ctx, server_id_t src_id, std::string address, bool accept,
                          replication::snapshot::async::install_context::ptr p_async_ctx)
{
    assert(ctx.state.snapshot.is_in_process);
    ctx.state.snapshot.is_in_process = false;

    // Protect the context state while invoking the replication callback
    if (p_async_ctx->term == ctx.term) {
        accept = replication::snapshot::install_callback(ctx, accept, p_async_ctx->snapshot);
        utils::send_append_entries_response(ctx, src_id, std::move(address), p_async_ctx->term, accept, p_async_ctx->last_log_index);
    }
}

} // <anonymous> namespace

void handle_request(context& ctx, server_id_t src_id, const std::string& address, term_t term, const snapshot_message& msg)
{
    RAFT_SH_LOG_TRACE(ctx, "Handle snapshot. Request from server %llu to server %llu(%s), current term %u",
        src_id, ctx.id, ctx.role.str(), ctx.term);

    // Term validation. Sec 7, Table 13: "1. Reply immediately if term < currentTerm"
    role::update_term(ctx, term);
    if (ctx.term > term) {
        RAFT_SH_LOG_DEBUG(ctx, "Handle snapshot. Local term %u is higher then request term %u", ctx.term, term);
        return utils::send_append_entries_response(ctx, src_id, address, ctx.term, false, ctx.log.last_index());
    }

    // Defensive assertions for internal state (Node must be a Follower or a Candidate that has reset its state)
    assert(ctx.role.is_follower() || ctx.role.is_candidate());
    assert(ctx.term == term);

    if (ctx.role.is_candidate()) {
        role::become_follower(ctx);
    }

    assert(ctx.role.is_follower());

    // Update the leader since the message term is valid.
    role::update_leader(ctx, src_id);
    timeout::election_restart_task(ctx);

    //const bool accept = replication::snapshot::install(ctx, msg.last_index, msg.last_term, *msg.conf, msg.conf_index, *msg.buffer);

    // Invoke the replication subsystem to apply the snapshot to the state machine and log. Implements
    // steps 6-8 of Table 13 (discarding the log up to msg.last_index and resetting the state machine).
    replication::snapshot::async::install_context::ptr p_async_ctx;
    const bool accept = replication::snapshot::install(ctx, msg.last_index, msg.last_term, msg.conf, msg.conf_index, msg.buffer, p_async_ctx);
    // Support for non-blocking asynchronous I/O
    if (accept && ctx.is_async_io && p_async_ctx) {
        scheduler::handler_type handler_fn = [&ctx, src_id, addr = address, p_async_ctx = std::move(p_async_ctx)] () -> void {
            RAFT_SH_LOG_TRACE(ctx, "Server %llu(%s) is installing %zu snapshot asynchronously.", ctx.id, ctx.role.str());
            // Perform blocking disk write outside the critical section
            const bool accept = ctx.p_io->set_snapshot(p_async_ctx->snapshot);
            ctx.schd.execute_strand([&ctx, src_id, addr = std::move(addr), accept, p_async_ctx = std::move(p_async_ctx)] () -> void {
                handle_request_async(ctx, src_id, std::move(addr), accept, p_async_ctx);
            });
        };
        ctx.state.snapshot.is_in_process = true;
        ctx.schd.execute_async(std::move(handler_fn));
        return;
    }
    // Synchronous execution path
    const index_t last_log_index = accept ? msg.last_index : ctx.state.last_stored;
    return utils::send_append_entries_response(ctx, src_id, address, ctx.term, accept, last_log_index);
}

void request(context& ctx, const peer& p)
{
    assert(ctx.role.is_leader());
    assert(p.id != ctx.id);

    // Retrieve the active snapshot from storage
    std::optional<raft::snapshot> p_sh = ctx.p_io->get_snapshot();

    RAFT_SH_LOG_TRACE(ctx, "Request to install snapshot to server %llu. Server %llu(%s), current term %u", p.id, ctx.id, ctx.role.str(), ctx.term);
    if (! p_sh) {
        return; // Snapshot is missing; transmission is impossible.
    }

    // Guard check in case the node's role changed while retrieving the snapshot.
    if (! ctx.role.is_leader()) {
        return;
    }

    // Transmitting the InstallSnapshot RPC over the network. According to the Specification, the
    // parameters passed are: term, leaderId, lastIncludedIndex, lastIncludedTerm, offset, data, done.
    utils::send_snapshot_request(ctx, p.id, p.address, ctx.term, std::move(*p_sh));
}

} // namespace snapshot
} // namespace details
} // namespace raft
} // namespace wstux
