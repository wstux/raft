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
#include "raft_le/details/handlers/vote_handler.h"
#include "raft_le/details/role/convert.h"
#include "raft_le/details/role/election.h"

namespace wstux {
namespace raft {
namespace le {
namespace details {
namespace timeout {
namespace {

/**
 *  \brief  Checks if the leader maintains active contact with a majority (quorum)
 *      of nodes.
 *  \param  ctx - current server state context.
 *  \return true if the leader has active contact with a quorum of voting nodes
 *      (including itself), otherwise false if the leader has lost contact with
 *      the quorum and must become a follower.
 *
 *  \details    Raft Paper, Section 6 (Cluster membership changes / Leader lease):
 *      "A leader steps down if it does not receive heartbeat responses from a
 *      majority of the cluster nodes within an election timeout period."
 *
 *      Used for Leader Lease management. If the leader detects that it
 *      is disconnected from the majority of cluster nodes, it must step down to
 *      prevent a split-brain scenario.
 */
bool check_contact_quorum(context& ctx)
{
    assert(ctx.role.is_leader());

    peer::list peers = peers::to_list(ctx);
    const size_t contacts = 1 + std::count_if(peers.begin(), peers.end(),
        [](peer::ptr& p) -> bool {
            const bool recent_recv = p->reset_recent_recv();
            return (p->is_voter() && recent_recv);
        });

    return contacts > peers::quorum_for_election(ctx);
}

size_t election_timeout_ms(context& ctx)
{
    return ctx.election_distribution(ctx.rand_engine);
}

/**
 *  \brief  Evaluates whether the leader has stopped communicating with this
 *      follower.
 *  \param  ctx - current server state context.
 *  \return true if the node is a follower, has a known leader, and the probe
 *      tracking time for this leader has expired, otherwise false if there is
 *      no active leader, or if the leader lease/probe is still valid.
 *
 *  \details    Raft Paper, Section 5.2 (Leader election): "A follower remains
 *      in the follower state as long as it receives valid RPCs from a leader
 *      or candidate."
 *
 *  \todo   In the standard Raft algorithm, followers do not have a separate
 *      heartbeat timeout. Followers only manage a single randomized election
 *      timeout. Having two independent timeout tasks (heartbeat and election)
 *      introduces synchronization vulnerabilities.
 */
/*bool is_leader_expired(context& ctx)
{
    assert(ctx.role.is_follower());

    if (ctx.role.follower_state.leader_id == gk_invalid_id) {
        return false;
    }

    peer::ptr p_leader_peer = peers::find(ctx, ctx.role.follower_state.leader_id);
    if (p_leader_peer) {
        return p_leader_peer->is_probe_expired();
    }
    return true;
}*/

} // <anonymous> namespace

void election_cancel_task(context& ctx)
{
    RAFT_TO_LOG_TRACE(ctx, "Election task cancel. " << ctx << ", current term " << ctx.term);
    ctx.p_scheduler->cancel(ctx.election_task);
}

void election_restart_task(context& ctx)
{
    ctx.p_scheduler->reschedule(ctx.election_task, election_timeout_ms(ctx));
}

void election_timeout_task(context& ctx)
{
    std::unique_lock<std::mutex> lock(ctx.handler_mutex);

    assert(ctx.role.is_voter);

    if (ctx.is_stop_fn()) {
        RAFT_TO_LOG_DEBUG(ctx, "Election task timeout. Server " << ctx << " has been stopped.");
        return;
    }

    RAFT_TO_LOG_TRACE(ctx, "Election task timeout. " << ctx << ", current term " << ctx.term);

    if (ctx.role.is_leader()) {
        // Raft Paper, Section 6 (Leader lease): "A leader steps down if it does
        // not receive heartbeat responses from a majority of the cluster nodes."
        if (! check_contact_quorum(ctx)) {
            role::become_follower(ctx);
        }
    } else if (ctx.role.is_candidate()) {
        // Raft Paper, Section 9.6 (Extending Pre-vote phase): "In the Pre-Vote
        // phase, a candidate tentatively increments its next term, but does not
        // advance its actual term until it receives approval from a quorum."
        role::election_start(ctx);
    } else if (ctx.role.is_follower()) {
        if (ctx.role.is_voter) {
            // Raft Paper, Section 5.2 (Leader election): "To begin an election,
            // a follower increments its current term and transitions to
            // candidate state."
            role::become_candidate(ctx);
        }
    }

    // Always restart the timer to trigger the next evaluation loop cycle.
    election_restart_task(ctx);
}

void heartbeat_cancel_task(context& ctx)
{
    RAFT_TO_LOG_TRACE(ctx, "Heartbeat task cancel. " << ctx << ", current term " << ctx.term);
    ctx.p_scheduler->cancel(ctx.heartbeat_task);
}

void heartbeat_restart_task(context& ctx)
{
    ctx.p_scheduler->reschedule(ctx.heartbeat_task, ctx.heartbeat_interval_ms);
}

void heartbeat_timeout_task(context& ctx)
{
    std::unique_lock<std::mutex> lock(ctx.handler_mutex);

    if (ctx.role.is_leader()) {
        // Raft Paper, Section 5.2: "Leaders send periodic heartbeats to maintain their authority."
        heartbeat::request(ctx);
    /*} else if (ctx.role.is_follower()) {
        // \todo: Design redundancy and specification violation.
        // Followers in standard Raft should not have an active heartbeat timeout
        // task. Forcing this task to reset leader_id to null independently of the
        // overall election timeout introduces race conditions and split-brain
        // scenarios caused by trivial network packet delays.
        if (is_leader_expired(ctx)) {
            RAFT_TO_LOG_DEBUG(ctx, "Heartbeat timeout task. Leader " << ctx.role.follower_state.leader_id
                << " for server " << ctx << " has been expired.");
            ctx.role.follower_state.leader_id = gk_invalid_id;
        }*/
    }
    // Reschedule the heartbeat task to maintain cyclic execution.
    heartbeat_restart_task(ctx);
}

} // namespace timeout
} // namespace details
} // namespace le
} // namespace raft
} // namespace wstux
