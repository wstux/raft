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

}

void handle_request(context& ctx, server_id_t src_id, const std::string& address, term_t term, const join_message& msg)
{
    RAFT_JOIN_LOG_TRACE(ctx, "Handle join. Request from server %llu to server %llu(%s).", src_id, ctx.id, ctx.role.str(), ctx.term);

    if (ctx.role.is_leader()) {
        server_config cfg(msg.id, msg.address, msg.is_voter);
        const bool accept = replication::membership::append(ctx, cfg);
        const join_status status = accept ? join_status::accept : join_status::failed;
        return utils::send_join_response(ctx, ctx.id, address, ctx.term, status);
    } else {
        const server_id_t leader_id = ctx.role.leader_id;
        if (leader_id == gk_invalid_id) {
            return utils::send_join_response(ctx, ctx.id, address, ctx.term, join_status::no_leader);
        }
        const server_config* p_cfg = utils::find_server_config(ctx, leader_id);
        if (p_cfg == nullptr) {
            return utils::send_join_response(ctx, ctx.id, address, ctx.term, join_status::no_leader);
        }
        utils::send_join_request(ctx, src_id, p_cfg->address, term, msg.id, msg.address, msg.is_voter);
        // else send false resp
    }
}

void handle_response(context& ctx, server_id_t src_id, const std::string& address, term_t /*term*/, const join_response_message& msg)
{
    RAFT_JOIN_LOG_TRACE(ctx, "Handle join response. Response from server %llu to server %llu(%s).", src_id, ctx.id, ctx.role.str());
    if (msg.status == join_status::accept) {
        RAFT_JOIN_LOG_TRACE(ctx, "Server %llu(%s) joined to cluster", address.c_str(), ctx.role.str());
        role::become_follower(ctx);
        role::update_leader(ctx, src_id);
    } else if (msg.status == join_status::no_leader || msg.status == join_status::busy_leader) {
        // repeat from timeout
    }
}

void request(context& ctx, const std::string& address)
{
    RAFT_JOIN_LOG_TRACE(ctx, "Sending request to server %s to join. Server %llu(%s), current term %u", address.c_str(), ctx.role.str(), ctx.term);
    return utils::send_join_request(ctx, gk_invalid_id, address, ctx.term, ctx.id, ctx.address, true);
}

} // namespace heartbeat
} // namespace details
} // namespace raft
} // namespace wstux
