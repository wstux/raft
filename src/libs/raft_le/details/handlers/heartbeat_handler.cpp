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

#include "raft_le/details/logging.h"
#include "raft_le/details/handlers/heartbeat_handler.h"
#include "raft_le/details/handlers/timeout_handler.h"
#include "raft_le/details/role/convert.h"

namespace wstux {
namespace raft {
namespace le {
namespace details {
namespace heartbeat {

void handle_request(context& ctx, term_t term, server_id_t src_id, const heartbeat_message& /*msg*/)
{
    peer::ptr p_src_peer = peers::find(ctx, src_id);
    if (! p_src_peer) {
        RAFT_HB_LOG_TRACE(ctx, "Server " << src_id << " does not exists");
        return;
    }

    RAFT_HB_LOG_TRACE(ctx, "Handle heartbeat. Request from server " << src_id << " to server " << ctx << ", current term " << ctx.term);

    p_src_peer->update_last_response();

    // Raft Paper, Section 5.1: If RPC request or response contains term T > currentTerm: set currentTerm = T
    role::update_term(ctx, term);
    // Raft Paper, Section 5.1: AppendEntries RPC: 1. Reply false if term < currentTerm
    if (ctx.term > term) {
        RAFT_HB_LOG_DEBUG(ctx, "Handle heartbeat. Local term " << ctx.term << " is higher then request term " << term);
        return utils::wrap_send(ctx, p_src_peer, &peer::send_heartbeat_response, ctx.term.load(), ctx.id, false);
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

    utils::wrap_send(ctx, p_src_peer, &peer::send_heartbeat_response, ctx.term.load(), ctx.id, true);
}

void handle_response(context& ctx, term_t term, server_id_t src_id, const heartbeat_response_message& /*msg*/)
{
    RAFT_HB_LOG_TRACE(ctx, "Handle heartbeat. Response from server " << src_id << " to server " << ctx << ", current term " << ctx.term);

    // Stale response that arrived after we already lost leadership
    if (! ctx.role.is_leader()) {
        return;
    }

    // Outdated response from previous terms — ignore
    if (ctx.term > term) {
        RAFT_HB_LOG_DEBUG(ctx, "Handle heartbeat response. Local term " << ctx.term << " is higher then request term " << term);
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
        RAFT_HB_LOG_DEBUG(ctx, "Got heartbeat response message from removed server " << src_id);
        return;
    }

    // Reset the node availability timeout
    p_src_peer->mark_recent_recv();
    p_src_peer->update_last_response();
}

void request(context& ctx)
{
    RAFT_HB_LOG_TRACE(ctx, "Request heartbeat. " << ctx << ", current term " << ctx.term);

    assert(ctx.role.is_leader());

    std::shared_lock<std::shared_mutex> lock(ctx.peers_mutex);
    for (const peer::map::value_type& v : ctx.peers) {
        const peer::list::value_type& p = v.second;

        assert(p->id() != ctx.id);

        RAFT_HB_LOG_TRACE(ctx, "Sending heartbeat request to server " << p->id() << ". " << ctx << ", current term " << ctx.term);
        utils::wrap_send(ctx, p, &peer::send_heartbeat_request, ctx.term.load(), ctx.id);
    }
}

} // namespace heartbeat
} // namespace details
} // namespace le
} // namespace raft
} // namespace wstux
