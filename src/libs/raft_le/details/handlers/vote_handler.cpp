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
#include "raft_le/details/handlers/vote_handler.h"
#include "raft_le/details/role/convert.h"
#include "raft_le/details/role/election.h"

namespace wstux {
namespace raft {
namespace le {
namespace details {
namespace vote {
namespace {

/**
 *  \brief  Checks conditions for granting a vote to a specific candidate.
 *  \param  ctx - server state context.
 *  \param  candidate_id - identifier of the candidate requesting the vote.
 *  \param  msg - vote request message structure.
 *  \return true if the candidate meets log up-to-date criteria and is available
 *      for voting, otherwise false if the node denies the vote.
 *
 *  \details    Implements Raft safety rules (Election Safety) described in the
 *      Raft Paper, Sections 5.2 and 5.4.1.
 *
 *      Implements Raft safety rules (Section 5.2 and 5.4):
 *      1. Checks if this server is eligible to vote (`is_voter`).
 *      2. Checks if a vote has already been given to another candidate in the
 *         current term.
 *      3. Compares log up-to-dateness (Election Restriction): the candidate's
 *         log must be at least as up-to-date as the voter's (`last_log_term`
 *         and `last_log_index`).
 */
bool got_vote(context& ctx, const server_id_t candidate_id, const vote_message& msg)
{
    assert(ctx.id != candidate_id);

    // The node must be a full cluster participant (voter).
    if (! ctx.role.is_voter) {
        RAFT_VOTE_LOG_DEBUG(ctx, "Voting. Server " << ctx << " is not voter. Doesn't cast his vote.");
        return false;
    }

    // Raft Section 5.2 & Figure 5.1 (RequestVote RPC):
    // The voted_for check is only important for real voting.
    if (! msg.is_prevote && ctx.role.voted_for != gk_invalid_id && ctx.role.voted_for != candidate_id) {
        RAFT_VOTE_LOG_DEBUG(ctx, "Voting. Server " << ctx << " already gave vote for server " << ctx.role.voted_for << ".");
        return false;
    }

    // Raft Dissertation, Section 9.6 (Pre-vote extension):
    // Pre-Vote requests must not modify the voted_for state or reset the
    // election timer, as they are speculative (preliminary).
    if (! msg.is_prevote) {
        RAFT_VOTE_LOG_DEBUG(ctx, "Server " << ctx << " granted vote for server " << candidate_id << ".");

        // Raft Paper, Figure 2 (State): voted_for in current term
        ctx.role.voted_for = candidate_id;
        // Raft Paper, Section 5.2: Restart timer when leader legitimacy is preserved
        //timeout::election_restart_task(ctx);
    }
    return true;
}

} // <anonymous> namespace

void handle_request(context& ctx, term_t term, server_id_t src_id, const vote_message& msg)
{
    RAFT_VOTE_LOG_DEBUG(ctx, "Handle " << (msg.is_prevote ? "prevote" : "vote")
        << ". Request from server " << src_id << " to server " << ctx << ", current term " << ctx.term);
    peer::ptr p_src_peer = peers::find(ctx, src_id);
    if (! p_src_peer) {
        RAFT_VOTE_LOG_DEBUG(ctx, "Got vote message from removed server " << src_id);
        return;
    }

    // Raft Dissertation & Pre-Vote specification:
    // For standard Vote, the return term is always the receiver's current term.
    // For Pre-Vote, the standard dictates returning the candidate's term if
    // rejected, or the receiver's current term if it's higher.
    term_t cur_term = (msg.is_prevote) ? term : ctx.term.load();

    // Raft Dissertation, Section 9.6 (Leader Stickiness / Pre-vote):
    // If a node believes the current leader is active (lease timer has not
    // expired/heartbeats are ongoing), it must reject any Pre-Vote requests to
    // protect the cluster from disruptions caused by partitioned nodes.
    if (ctx.role.is_leader() || ctx.role.has_leader()) {
        utils::wrap_send(ctx, p_src_peer, &peer::send_vote_response, cur_term, ctx.id, msg.is_prevote, false);
        return;
    }

    // If this is a pre-vote request, don't actually increment our term or persist the vote.
    if (! msg.is_prevote) {
        role::update_term(ctx, term);

        // Raft Dissertation & Pre-Vote specification:
        // For standard Vote, the return term is always the receiver's current term.
        // For Pre-Vote, the standard dictates returning the candidate's term if
        // rejected, or the receiver's current term if it's higher.
        cur_term = ctx.term.load();
    }

    if (ctx.term > term) {
        RAFT_VOTE_LOG_DEBUG(ctx, (msg.is_prevote ? "Prevote" : "Vote") << " request. Server "
            << ctx << ". Local term (" << ctx.term << ") is higher than source term (" << term << ").");
        utils::wrap_send(ctx, p_src_peer, &peer::send_vote_response, cur_term, ctx.id, msg.is_prevote, false);
        return;
    }

    if (! msg.is_prevote) {
        assert(ctx.term == term);
    }

    const bool accept = got_vote(ctx, src_id, msg);
    RAFT_VOTE_LOG_DEBUG(ctx, (msg.is_prevote ? "Prevote" : "Vote") << " request. Server " << ctx
        << (accept ? "" : " not") << " supported candidate " << src_id << " at the "
        << (msg.is_prevote ? "prevote" : "vote") << ".");

    utils::wrap_send(ctx, p_src_peer, &peer::send_vote_response, cur_term, ctx.id, msg.is_prevote, accept);
}

void handle_response(context& ctx, term_t term, server_id_t src_id, const vote_response_message& msg)
{
    RAFT_VOTE_LOG_DEBUG(ctx, "Handle " << (msg.is_prevote ? "prevote" : "vote")
        << " response. Response from server " << src_id << ". " << ctx << ", current term " << ctx.term);

    // Raft Paper, Section 5.2: A candidate can only process responses as long
    // as it remains a candidate.
    if (! ctx.role.is_candidate()) {
        return;
    }

    assert(ctx.role.is_candidate());

    peer::ptr p_src_peer = peers::find(ctx, src_id);
    if (! p_src_peer) {
        RAFT_VOTE_LOG_DEBUG(ctx, "Got vote response message from removed server " << src_id);
        return;
    }

    // Raft Paper, Figure 2 (Rules for Servers): If the incoming term is greater
    // than ours, we revert to Follower.
    if (! ctx.role.candidate_state.is_prevote) {
        role::update_term(ctx, term);
        if (! ctx.role.is_candidate()) {
            return;
        }
    }

    // Raft Paper, Section 5.1: If the response comes from an older term, it
    // must be ignored immediately.
    if (ctx.term > term) {
        RAFT_VOTE_LOG_DEBUG(ctx, (msg.is_prevote ? "Prevote" : "Vote") << " response. Response from server "
            << src_id << " to server " << ctx << ". Local term (" << ctx.term
            << ") is higher than source term (" << term << ").");
        return;
    }

    // Avoid counting pre-vote votes as regular votes.
    if (msg.is_prevote != ctx.role.candidate_state.is_prevote) {
        return;
    }

    // Pre-Vote specification (term validation): During Pre-Vote, the node's
    // `ctx.term` is NOT incremented, so it expects responses to match its current
    // term or be from a future perspective.
    if (ctx.role.candidate_state.is_prevote) {
        if (term > ctx.term + 1) {
            assert(! msg.accept);
            RAFT_VOTE_LOG_DEBUG(ctx, "Prevote response. Server " << ctx << " has local term ("
                << ctx.term << ") lass than source term (" << term << ").");
            role::update_term(ctx, term);
            return;
        }
    } else {
        // For real votes, terms must match exactly since we step down above
        // if `term > ctx.term`.
        assert(ctx.term == term);
    }

    RAFT_VOTE_LOG_DEBUG(ctx, (msg.is_prevote ? "Prevote" : "Vote") << " response. Server "
        << ctx << (msg.accept ? " got" : " did not get") << " vote from server " << src_id << ".");

    if (msg.accept) {
        // Raft Paper, Section 5.2: Candidate receives a vote from a network node.
        ++ctx.role.candidate_state.votes_granted;

        // Raft Paper, Section 5.2: If a candidate wins a majority of votes from
        // the cluster nodes, it wins the election.
        // Check if a majority (quorum) has been reached: (N/2) + 1
        if (role::election_results(ctx)) {
            if (ctx.role.candidate_state.is_prevote) {
                RAFT_VOTE_LOG_DEBUG(ctx, "Votes quorum reached. Prevote successful. " << ctx << ", current term " << ctx.term);
                // Raft Dissertation, Section 9.6: A successful Pre-Vote allows
                // the candidate to officially increment the term and start the
                // real election.
                ctx.role.candidate_state.is_prevote = false;
                role::election_start(ctx);
            } else {
                RAFT_VOTE_LOG_DEBUG(ctx, "Votes quorum reached. Convert to leader. " << ctx << ", current term " << ctx.term);

                // Raft Paper, Section 5.2: A candidate wins the election if it
                // receives votes from a majority of servers.
                role::become_leader(ctx);

                // Raft Paper, Section 5.2: Upon election, a leader must immediately
                // send initial empty AppendEntries RPCs (heartbeats) to all peers
                // to establish authority and prevent other elections.
                // Broadcast empty AppendEntries to assert authority (Figure 2)
                heartbeat::request(ctx);
            }
        }
    }
}

void request(context& ctx)
{
    assert(ctx.role.is_candidate());

    RAFT_VOTE_LOG_DEBUG(ctx, "Request " << (ctx.role.candidate_state.is_prevote ? "prevote" : "vote")
        << ". " << ctx << ", current term " << ctx.term);

    // Raft Paper, Section 5.2: For a real vote: The server MUST increment its
    // current term (`ctx.term++`) before starting the election. For a pre-vote:
    // The server speculatively increments the term inside the RPC payload
    // to check future viability, but its local `ctx.term` MUST remain unchanged.
    term_t term = ctx.term;
    // Raft Dissertation, Section 9.6: During Pre-Vote, the term is checked as
    // incremented on the network, but locally on the node, ctx.term is NOT
    // increased until quorum is confirmed.
    if (ctx.role.candidate_state.is_prevote) {
        ++term;
    }

    // Raft Paper, Section 5.2: "Each candidate votes for itself..."
    ctx.role.candidate_state.votes_granted = 1;
    //timeout::election_restart_task(ctx);

    const bool is_prevote = ctx.role.candidate_state.is_prevote;

    /*peer::list peers = peers::to_list(ctx);
    for (peer::ptr& p : peers) {
        if (p->is_voter()) {
            utils::wrap_send(ctx, p, &peer::send_vote_request, term, ctx.id, is_prevote);
        }
    }*/
    std::shared_lock<std::shared_mutex> lock(ctx.peers_mutex);
    for (const peer::map::value_type& v : ctx.peers) {
        if (v.second->is_voter()) {
            utils::wrap_send(ctx, v.second, &peer::send_vote_request, term, ctx.id, is_prevote);
        }
    }
}

} // namespace vote
} // namespace details
} // namespace le
} // namespace raft
} // namespace wstux
