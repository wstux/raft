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
#include "raft/details/handlers/heartbeat_handler.h"
#include "raft/details/handlers/timeout_handler.h"
#include "raft/details/handlers/vote_handler.h"
#include "raft/details/role/convert.h"
#include "raft/details/role/election.h"

namespace wstux {
namespace raft {
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
        RAFT_VOTE_LOG_DEBUG(ctx, "Voting. Server %llu(%s) is not voter. Doesn't cast his vote.", ctx.id, ctx.role.str());
        return false;
    }

    // Raft Section 5.2 & Figure 5.1 (RequestVote RPC):
    // The voted_for check is only important for real voting.
    if (! msg.is_prevote && ctx.role.voted_for != gk_invalid_id && ctx.role.voted_for != candidate_id) {
        RAFT_VOTE_LOG_DEBUG(ctx, "Voting. Server %llu(%s) already gave vote for server %llu.", ctx.id, ctx.role.str(), ctx.role.voted_for);
        return false;
    }

    // Raft Dissertation, Section 9.6 (Pre-vote extension):
    // Pre-Vote requests must not modify the voted_for state or reset the
    // election timer, as they are speculative (preliminary).
    if (! msg.is_prevote) {
        RAFT_VOTE_LOG_DEBUG(ctx, "Server %llu(%s) granted vote for server %llu.", ctx.id, ctx.role.str(), candidate_id);

        // Raft Paper, Figure 2 (State): voted_for in current term
        ctx.role.voted_for = candidate_id;
        // Raft Paper, Section 5.2: Restart timer when leader legitimacy is preserved
        timeout::election_restart_task(ctx);
    }
    return true;
}

} // <anonymous> namespace

void handle_request(context& ctx, term_t term, server_id_t src_id, const vote_message& msg)
{
    RAFT_VOTE_LOG_DEBUG(ctx, "Handle %s. Request from server %llu to server %llu(%s), current term %u",
        (msg.is_prevote ? "prevote" : "vote"), src_id, ctx.id, ctx.role.str(), ctx.term);
    peer::ptr p_src = peers::find(ctx, src_id);
    if (! p_src) {
        RAFT_VOTE_LOG_DEBUG(ctx, "Got vote message from removed server %llu", src_id);
        return;
    }

    // Raft Dissertation & Pre-Vote specification:
    // For standard Vote, the return term is always the receiver's current term.
    // For Pre-Vote, the standard dictates returning the candidate's term if
    // rejected, or the receiver's current term if it's higher.
    term_t cur_term = (msg.is_prevote) ? term : ctx.term;

    // Raft Dissertation, Section 9.6 (Leader Stickiness / Pre-vote):
    // If a node believes the current leader is active (lease timer has not
    // expired/heartbeats are ongoing), it must reject any Pre-Vote requests to
    // protect the cluster from disruptions caused by partitioned nodes.
    if (ctx.role.is_leader() || ctx.role.has_leader()) {
        return utils::send<message_type::vote_response>(ctx, p_src->id, cur_term, ctx.id, msg.is_prevote, false);
    }

    // If this is a pre-vote request, don't actually increment our term or persist the vote.
    if (! msg.is_prevote) {
        role::update_term(ctx, term);

        // Raft Dissertation & Pre-Vote specification:
        // For standard Vote, the return term is always the receiver's current term.
        // For Pre-Vote, the standard dictates returning the candidate's term if
        // rejected, or the receiver's current term if it's higher.
        cur_term = ctx.term;
    }

    if (ctx.term > term) {
        RAFT_VOTE_LOG_DEBUG(ctx, "%s request. Server %llu(%s). Local term (%u) is higher than source term (%u).",
            (msg.is_prevote ? "Prevote" : "Vote"), ctx.id, ctx.role.str(), ctx.term, term);
        return utils::send<message_type::vote_response>(ctx, p_src->id, cur_term, ctx.id, msg.is_prevote, false);
    }

    if (! msg.is_prevote) {
        assert(ctx.term == term);
    }

    const bool accept = got_vote(ctx, src_id, msg);
    RAFT_VOTE_LOG_DEBUG(ctx, "%s request. Server %llu(%s)%s supported candidate %llu at the %s.",
        (msg.is_prevote ? "Prevote" : "Vote"), ctx.id, ctx.role.str(), (accept ? "" : " not"),
        src_id, (msg.is_prevote ? "prevote" : "vote"));

    utils::send<message_type::vote_response>(ctx, p_src->id, cur_term, ctx.id, msg.is_prevote, accept);
}

void handle_response(context& ctx, term_t term, server_id_t src_id, const vote_response_message& msg)
{
    RAFT_VOTE_LOG_DEBUG(ctx, "Handle %s response. Response from server %llu to server %llu(%s), current term %u",
        (msg.is_prevote ? "prevote" : "vote"), src_id, ctx.id, ctx.role.str(), ctx.term);

    // Raft Paper, Section 5.2: A candidate can only process responses as long
    // as it remains a candidate.
    if (! ctx.role.is_candidate()) {
        return;
    }

    assert(ctx.role.is_candidate());

    peer::ptr p_src_peer = peers::find(ctx, src_id);
    if (! p_src_peer) {
        RAFT_VOTE_LOG_DEBUG(ctx, "Got vote response message from removed server %llu", src_id);
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
        RAFT_VOTE_LOG_DEBUG(ctx, "%s response. Response from server %llu to server %llu(%s). "
            "Local term (%u) is higher than source term (%u).",
            (msg.is_prevote ? "Prevote" : "Vote"), src_id, ctx.id, ctx.role.str(), ctx.term, term);
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
            RAFT_VOTE_LOG_DEBUG(ctx, "Prevote response. Server %llu(%s) has local term (%u) lass than source term (%u).",
                ctx.id, ctx.role.str(), ctx.term, term);
            role::update_term(ctx, term);
            return;
        }
    } else {
        // For real votes, terms must match exactly since we step down above
        // if `term > ctx.term`.
        assert(ctx.term == term);
    }

    RAFT_VOTE_LOG_DEBUG(ctx, "%s response. Server %llu(%s) %s vote from server %llu.",
        (msg.is_prevote ? "Prevote" : "Vote"), ctx.id, ctx.role.str(), (msg.accept ? "got" : "did not get"), src_id);

    if (msg.accept) {
        // Raft Paper, Section 5.2: Candidate receives a vote from a network node.
        ++ctx.role.candidate_state.votes_granted;

        // Raft Paper, Section 5.2: If a candidate wins a majority of votes from
        // the cluster nodes, it wins the election.
        // Check if a majority (quorum) has been reached: (N/2) + 1
        if (role::election_results(ctx)) {
            if (ctx.role.candidate_state.is_prevote) {
                RAFT_VOTE_LOG_DEBUG(ctx, "Votes quorum reached. Prevote successful. Server %llu(%s), current term %u",
                    ctx.id, ctx.role.str(), ctx.term);
                // Raft Dissertation, Section 9.6: A successful Pre-Vote allows
                // the candidate to officially increment the term and start the
                // real election.
                ctx.role.candidate_state.is_prevote = false;
                role::election_start(ctx);
            } else {
                RAFT_VOTE_LOG_DEBUG(ctx, "Votes quorum reached. Convert to leader. Server %llu(%s), current term %u",
                    ctx.id, ctx.role.str(), ctx.term);

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

    RAFT_VOTE_LOG_DEBUG(ctx, "Request %s. Server %llu(%s), current term %u",
        (ctx.role.candidate_state.is_prevote ? "prevote" : "vote"), ctx.id, ctx.role.str(), ctx.term);

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
    timeout::election_restart_task(ctx);

    const bool is_prevote = ctx.role.candidate_state.is_prevote;

    for (const peer& p : ctx.peers) {
        if (p.is_voter) {
            utils::send<message_type::vote_request>(ctx, p.id, term, ctx.id, is_prevote);
        }
    }
}

} // namespace vote
} // namespace details
} // namespace raft
} // namespace wstux
