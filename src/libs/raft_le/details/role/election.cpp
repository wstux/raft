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

#include "raft_le/details/logging.h"
#include "raft_le/details/handlers/timeout_handler.h"
#include "raft_le/details/handlers/vote_handler.h"
#include "raft_le/details/role/convert.h"
#include "raft_le/details/role/election.h"

namespace wstux {
namespace raft {
namespace le {
namespace details {
namespace role {

bool election_results(context& ctx)
{
    assert(ctx.role.is_candidate());

    const size_t quorum_size = peers::quorum_for_election(ctx) + 1;
    const size_t votes = ctx.role.candidate_state.votes_granted;

    return ctx.role.is_candidate() && (votes >= quorum_size);
}

void election_start(context& ctx)
{
    assert(ctx.role.is_candidate());

    if (! ctx.role.candidate_state.is_prevote) {
        term_t term = ++ctx.term;
        RAFT_ROOT_LOG_TRACE(ctx, "Server " << ctx << " started election with local increased term " << ctx.term);
        ctx.p_io->set_term(term);
        ctx.role.voted_for = ctx.id;
    }

    timeout::election_restart_task(ctx);
    vote::request(ctx);
}

void initiate_election(context& ctx)
{
    assert(ctx.role.is_follower());

    if (! ctx.role.is_voter) {
        return;
    }

    if (peers::quorum_for_election(ctx) == 0) {
        role::become_candidate(ctx);
    }
}

} // namespace role
} // namespace details
} // namespace le
} // namespace raft
} // namespace wstux
