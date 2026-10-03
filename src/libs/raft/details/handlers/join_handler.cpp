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

#include "raft/details/logger.h"
#include "raft/details/connection/send.h"
#include "raft/details/handlers/join_handler.h"
#include "raft/details/replication/membership.h"
#include "raft/details/role/convert.h"

namespace wstux {
namespace raft {
namespace details {
namespace join {
namespace {

enum join_status : uint32_t
{
    accept = 0,
    failed = 1,
    no_leader = 2,
    busy_leader = 3,
};

} // <anonymous> namespace

void handle_request(context& ctx, server_id_t src_id, const std::string& /*address*/, term_t term, const join_message& msg)
{
    RAFT_JOIN_LOG_DEBUG(ctx, "Handle join. Request from server %llu with address %s to server %llu(%s).",
        src_id, msg.address.c_str(), ctx.id, ctx.role.str());

    if (ctx.role.is_leader()) {
        server_config cfg(msg.id, msg.address, msg.is_voter);
        const bool accept = replication::membership::append(ctx, cfg);
        const join_status status = accept ? join_status::accept : join_status::failed;
        RAFT_JOIN_LOG_DEBUG(ctx, "Handle join. Leader %llu(%s) has joined server %llu to cluster as %s.",
            ctx.id, ctx.role.str(), msg.id, (msg.is_voter ? "voter" : "non-voter"));
        return utils::send_join_response(ctx, src_id, msg.address, ctx.term, status);
    } else {
        const server_config* p_cfg = utils::find_server_config(ctx, ctx.role.leader_id);
        if (ctx.role.leader_id == gk_invalid_id || p_cfg == nullptr) {
            RAFT_JOIN_LOG_ERROR(ctx, "Reject join request from server %llu. Reason: cluster does not have leader", msg.id);
            return utils::send_join_response(ctx, src_id, msg.address, ctx.term, join_status::no_leader);
        }
        RAFT_JOIN_LOG_DEBUG(ctx, "Resend join request to leader %llu.", p_cfg->id);
        utils::send_join_request(ctx, p_cfg->id, p_cfg->address, term, msg.id, msg.address, msg.is_voter);
    }
}

void handle_response(context& ctx, server_id_t src_id, const std::string& address, term_t /*term*/, const join_response_message& msg)
{
    RAFT_JOIN_LOG_DEBUG(ctx, "Handle join response. Response from server %llu to server %llu(%s).", src_id, ctx.id, ctx.role.str());
    if (msg.status == join_status::accept) {
        RAFT_JOIN_LOG_TRACE(ctx, "Server %llu(%s) joined to cluster with leader %s", ctx.id, ctx.role.str(), address.c_str());
        //role::become_follower(ctx);
        //role::update_leader(ctx, src_id);
    } else if (msg.status == join_status::no_leader || msg.status == join_status::busy_leader) {
        RAFT_JOIN_LOG_WARN(ctx, "Failed to join cluster. Repeat request in 100 ms.");
        ctx.schd.schedule([&ctx, leader_addr = address]() { request(ctx, leader_addr); }, 100);
    }
}

void request(context& ctx, const std::string& address)
{
    RAFT_JOIN_LOG_DEBUG(ctx, "Sending request to server %s to join. Server %llu(%s)", address.c_str(), ctx.id, ctx.role.str());
    return utils::send_join_request(ctx, gk_invalid_id, address, ctx.term, ctx.id, ctx.address, ctx.role.is_voter);
}

} // namespace join
} // namespace details
} // namespace raft
} // namespace wstux
