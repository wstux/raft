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

#ifndef _LIBS_RAFT_HANDLERS_HEARTBEAT_HANDLER_H_
#define _LIBS_RAFT_HANDLERS_HEARTBEAT_HANDLER_H_

#include "raft/details/context.h"
#include "raft/details/connection/messages.h"

namespace wstux {
namespace raft {
namespace details {
namespace heartbeat {

void handle_request(context& ctx, term_t term, server_id_t src_id, const heartbeat_message& msg);

void handle_response(context& ctx, term_t term, server_id_t src_id, const heartbeat_response_message& msg);

/**
 *  \brief  Initiates health checks (heartbeats) to all followers.
 *  \param  ctx - current server state context.
 *
 *  \details    Raft Paper, Section 5.3 "Log replication": "Receiver implementation:
 *      1. Reply false if term < currentTerm..." "Leaders: Send empty AppendEntries
 *      RPCs (heartbeat) to each server; repeat during periods of idle period to
 *      prevent election timeouts."
 *
 *      This method is triggered by the leader's timer to maintain its authority
 *      and prevent followers from transitioning to the Candidate state.
 *
 *  \todo   Remove the remove_expired(ctx) call. Periodic heartbeat dispatches
 *      must not cause cluster degradation or membership changes.
 */
void request(context& ctx);

} // namespace heartbeat
} // namespace details
} // namespace raft
} // namespace wstux

#endif /* _LIBS_RAFT_HANDLERS_HEARTBEAT_HANDLER_H_ */
