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
#include "raft/details/handlers/timeout_handler.h"
#include "raft/details/role/convert.h"
#include "raft/details/role/election.h"

namespace wstux {
namespace raft {
namespace details {
namespace role {

void become_follower(context& ctx)
{
    RAFT_LOG_INFO(ctx, "Server %llu(%s) is becoming follower, term %u", ctx.id, ctx.role.str(), ctx.term);

    ctx.role.role = role_type::follower;
    ctx.role.follower_state.leader_id = gk_invalid_id;
    ctx.role.voted_for = gk_invalid_id;
}

void become_candidate(context& ctx)
{
    RAFT_LOG_INFO(ctx, "Server %llu(%s) is becoming candidate, term %u", ctx.id, ctx.role.str(), ctx.term);

    assert(ctx.role.is_follower());
    assert(ctx.role.is_voter);

    ctx.role.role = role_type::candidate;

    ctx.role.candidate_state.votes_granted = 0;
    ctx.role.candidate_state.is_prevote = true;

    if (peers::voting_members_count(ctx) == 1) {
        become_leader(ctx);
    } else {
        election_start(ctx);
    }
}

void become_leader(context& ctx)
{
    RAFT_LOG_INFO(ctx, "Server %llu(%s) is becoming leader, term %u", ctx.id, ctx.role.str(), ctx.term);

    assert(ctx.role.is_candidate());

    ctx.role.role = role_type::leader;
}

void update_leader(context& ctx, server_id_t leader_id)
{
    assert(ctx.role.is_follower());

    if (ctx.role.follower_state.leader_id != leader_id) {
        RAFT_LOG_INFO(ctx, "Updating leader for server %llu(%s) to server with id %llu",
            ctx.id, ctx.role.str(), leader_id);
        ctx.role.follower_state.leader_id = leader_id;
    }
    timeout::election_restart_task(ctx);
}

void update_term(context& ctx, term_t term)
{
    if (term > ctx.term) {
        RAFT_LOG_TRACE(ctx, "Updating term for server %llu(%s) with local term %u to term %u",
            ctx.id, ctx.role.str(), ctx.term, term);
        ctx.p_io->set_term(term);

        ctx.term = term;
        ctx.role.voted_for = gk_invalid_id;

        if (! ctx.role.is_follower()) {
            become_follower(ctx);
        }
    }
}

} // namespace role
} // namespace details
} // namespace raft
} // namespace wstux
