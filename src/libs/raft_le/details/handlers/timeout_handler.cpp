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

#include "raft_le/details/logger.h"
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

size_t election_timeout_ms(context& ctx)
{
    return ctx.election_distribution(ctx.rand_engine);
}

} // <anonymous> namespace

void election_cancel_task(context& ctx)
{
    RAFT_TO_LOG_DEBUG(ctx, "Election task cancel. %llu(%s), current term %u", ctx.id, ctx.role.str(), ctx.term);
    ctx.p_scheduler->cancel(ctx.election_task);
}

void election_restart_task(context& ctx)
{
    ctx.p_scheduler->reschedule(ctx.election_task, election_timeout_ms(ctx));
}

void election_timeout_task(context& ctx)
{
    assert(ctx.role.is_voter);

    if (ctx.is_stop_fn()) {
        RAFT_TO_LOG_DEBUG(ctx, "Election task timeout. Server %llu(%s) has been stopped.", ctx.id, ctx.role.str());
        return;
    }

    RAFT_TO_LOG_TRACE(ctx, "Election task timeout. %llu(%s), current term %u", ctx.id, ctx.role.str(), ctx.term);

    if (ctx.role.is_leader()) {
        // Raft Paper, Section 6 (Leader lease): "A leader steps down if it does
        // not receive heartbeat responses from a majority of the cluster nodes."
        if (! peers::check_contact_quorum(ctx)) {
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
            // a follower increments its current term and transitions to candidate state."
            role::become_candidate(ctx);
        }
    }

    // Always restart the timer to trigger the next evaluation loop cycle.
    election_restart_task(ctx);
}

void heartbeat_cancel_task(context& ctx)
{
    RAFT_TO_LOG_DEBUG(ctx, "Heartbeat task cancel. %llu(%s), current term %u", ctx.id, ctx.role.str(), ctx.term);
    ctx.p_scheduler->cancel(ctx.heartbeat_task);
}

void heartbeat_restart_task(context& ctx)
{
    ctx.p_scheduler->reschedule(ctx.heartbeat_task, ctx.heartbeat_interval_ms);
}

void heartbeat_timeout_task(context& ctx)
{
    if (ctx.role.is_leader()) {
        // Raft Paper, Section 5.2: "Leaders send periodic heartbeats to maintain their authority."
        heartbeat::request(ctx);
    }
    // Reschedule the heartbeat task to maintain cyclic execution.
    heartbeat_restart_task(ctx);
}

} // namespace timeout
} // namespace details
} // namespace le
} // namespace raft
} // namespace wstux
