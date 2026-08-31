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

#ifndef _LIBS_RAFT_HANDLERS_APPEND_ENTRIES_HANDLER_H_
#define _LIBS_RAFT_HANDLERS_APPEND_ENTRIES_HANDLER_H_

#include "raft/io.h"
#include "raft/details/context.h"
#include "raft/details/connection/messages.h"
#include "raft/details/connection/peer.h"

namespace wstux {
namespace raft {
namespace details {
namespace append_entries {

/**
 *  \brief  Handler for incoming AppendEntries RPC from the leader (Follower/Candidate side).
 *  \param  ctx - current server state context.
 *  \param  term - term of the leader that sent the message.
 *  \param  src_id - identifier of the leader.
 *  \param  msg - message.
 *
 *  \details    Raft Paper, Section 5.1, 5.2, 5.3: Implements the following logic:
 *      1. Term verification (5.1).
 *      2. Reverting candidate state upon discovering a legitimate leader (5.2).
 *      3. Log consistency checks and writing new data (5.3).
 */
void handle_request(context& ctx, term_t term, server_id_t src_id, const append_entries_message& msg);

/**
 *  \brief  Handler for AppendEntries RPC responses (Leader side).
 *  \param  ctx - current server state context.
 *  \param  term - term of the remote node that sent the response.
 *  \param  src_id - identifier of the remote node (follower).
 *  \param  msg - message.
 *
 *  \details    Processes replication results: either advances tracking indices
 *  (matchIndex, nextIndex), or rolls them back in case of log inconsistency.
 */
void handle_response(context& ctx, term_t term, server_id_t src_id, const append_entries_response_message& msg);

/**
 *  \brief  Sends a broadcast AppendEntries request to all peers.
 *  \param  ctx - current server state context.
 *
 *  \details    Used as a Heartbeat (empty set of entries) or for data replication.
 *      Raft Paper, Section 5.2: "Leaders send periodic heartbeat-messages
 *      (AppendEntries RPCs with no log entries)"
 */
void request(context& ctx);

/**
 *  \brief  Calculates indices and sends AppendEntries/Snapshot to a specific node.
 *  \param  ctx - current server state context.
 *  \param  p - target node to which the request is sent.
 *
 *  \detials    Based on the follower's next_index, determines whether to send
 *      an incremental log or if the node is hopelessly lagging and requires a
 *      state snapshot (Snapshot).
 */
void request(context& ctx, const peer& p);

} // namespace append_entries
} // namespace details
} // namespace raft
} // namespace wstux

#endif /* _LIBS_RAFT_HANDLERS_APPEND_ENTRIES_HANDLER_H_ */
