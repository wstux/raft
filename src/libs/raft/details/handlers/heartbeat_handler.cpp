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
#include "raft/details/handlers/heartbeat_handler.h"
#include "raft/details/handlers/timeout_handler.h"
#include "raft/details/role/convert.h"

namespace wstux {
namespace raft {
namespace details {
namespace heartbeat {

void handle_request(context& ctx, term_t term, server_id_t src_id, const heartbeat_message& /*msg*/)
{
    peer::ptr p_src = peers::find(ctx, src_id);
    if (! p_src) {
        RAFT_HB_LOG_TRACE(ctx, "Server %llu does not exists", src_id);
        return;
    }

    RAFT_HB_LOG_TRACE(ctx, "Handle heartbeat. Request from server %llu to server %llu(%s), current term %u",
        src_id, ctx.id, ctx.role.str(), ctx.term);

    // Raft Paper, Section 5.1: If RPC request or response contains term T > currentTerm: set currentTerm = T
    role::update_term(ctx, term);
    // Raft Paper, Section 5.1: AppendEntries RPC: 1. Reply false if term < currentTerm
    if (ctx.term > term) {
        RAFT_HB_LOG_DEBUG(ctx, "Handle heartbeat. Local term %u is higher then request term %u", ctx.term, term);
        return utils::send<message_type::heartbeat_response>(ctx, p_src->id, ctx.term, ctx.id, false);
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

    utils::send<message_type::heartbeat_response>(ctx, p_src->id, ctx.term, ctx.id, true);
}

void handle_response(context& ctx, term_t term, server_id_t src_id, const heartbeat_response_message& /*msg*/)
{
    RAFT_HB_LOG_TRACE(ctx, "Handle heartbeat. Response from server %llu to server %llu(%s), current term %u",
        src_id, ctx.id, ctx.role.str(), ctx.term);

    // Stale response that arrived after we already lost leadership
    if (! ctx.role.is_leader()) {
        return;
    }

    // Outdated response from previous terms — ignore
    if (ctx.term > term) {
        RAFT_HB_LOG_DEBUG(ctx, "Handle heartbeat response. Local term %u is higher then request term %u", ctx.term, term);
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

    peer::ptr p_src = peers::find(ctx, src_id);
    if (! p_src) {
        RAFT_HB_LOG_DEBUG(ctx, "Got heartbeat response message from removed server %llu", src_id);
        return;
    }

    // Reset the node availability timeout
    p_src->mark_recent_recv();
}

void request(context& ctx)
{
    RAFT_HB_LOG_TRACE(ctx, "Request heartbeat. %llu(%s), current term %u", ctx.id, ctx.role.str(), ctx.term);

    assert(ctx.role.is_leader());

    for (const peer& p : ctx.peers) {
        assert(p.id != ctx.id);

        RAFT_HB_LOG_TRACE(ctx, "Sending heartbeat request to server %llu. %llu(%s), current term %u", p.id, ctx.id, ctx.role.str(), ctx.term);
        utils::send<message_type::heartbeat_request>(ctx, p.id, ctx.term, ctx.id);
    }
}

} // namespace heartbeat
} // namespace details
} // namespace raft
} // namespace wstux
