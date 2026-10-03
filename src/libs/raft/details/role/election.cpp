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
#include "raft/details/handlers/vote_handler.h"
#include "raft/details/role/convert.h"
#include "raft/details/role/election.h"

namespace wstux {
namespace raft {
namespace details {
namespace role {
namespace {

size_t server_index(const context& ctx, const server_id_t id)
{
    size_t idx = ctx.state.cluster_cfg.servers.size();
    for (size_t i = 0; i < ctx.state.cluster_cfg.servers.size(); ++ i) {
        if (ctx.state.cluster_cfg.servers[i].id == id) {
            idx = i;
            break;
        }
    }
    return idx;
}

} // <anonymous> namespace

size_t election_granted_votes(const context& ctx)
{
    assert(ctx.role.is_candidate());
    return std::count(ctx.role.candidate.votes.cbegin(), ctx.role.candidate.votes.cend(), true);
}

void election_process(context& ctx, const server_id_t id)
{
    assert(ctx.role.is_candidate());
    assert(ctx.role.candidate.votes.size() == ctx.state.cluster_cfg.servers.size());

    const size_t idx = server_index(ctx, id);
    if (idx != ctx.state.cluster_cfg.servers.size()) {
        ctx.role.candidate.votes[idx] = true;
    }
}

bool election_results(const context& ctx)
{
    assert(ctx.role.is_candidate());

    const size_t quorum_size = utils::quorum_for_election(ctx) + 1;
    const size_t votes = election_granted_votes(ctx);

    return ctx.role.is_candidate() && (votes >= quorum_size);
}

void election_start(context& ctx)
{
    assert(ctx.role.is_candidate());

    if (! ctx.role.candidate.is_prevote) {
        term_t term = ++ctx.term;
        RAFT_LOG_INFO(ctx, "Server %llu(%s) started election with local increased term %u", ctx.id, ctx.role.str(), ctx.term);
        ctx.p_io->set_term(term);
        ctx.p_io->set_voted_for(ctx.id);
        ctx.role.voted_for = ctx.id;
    }

    timeout::election_restart_task(ctx);
    vote::request(ctx);
}

void initiate_self_election(context& ctx)
{
    assert(ctx.role.is_follower());

    if (! ctx.role.is_voter) {
        return;
    }

    //if (utils::quorum_for_election(ctx) == 1) {
    //    role::become_candidate(ctx);
    //}
    if (utils::is_in_cluster(ctx, ctx.id) && details::utils::voting_members_count(ctx) == 1) {
        details::role::become_candidate(ctx);
    }
}

} // namespace role
} // namespace details
} // namespace raft
} // namespace wstux
