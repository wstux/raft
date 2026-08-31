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

#ifndef _LIBS_RAFT_SNAPSHOT_HANDLER_H_
#define _LIBS_RAFT_SNAPSHOT_HANDLER_H_

#include "raft/io.h"
#include "raft/details/context.h"
#include "raft/details/connection/messages.h"
#include "raft/details/connection/peer.h"

namespace wstux {
namespace raft {
namespace details {
namespace snapshot {

/**
 *  \brief  Processes an incoming snapshot installation request from the leader.
 *  \param  ctx - current server state context.
 *  \param  term - term (epoch) of the leader that sent the snapshot.
 *  \param  src_id - identifier of the leader.
 *  \param  msg - message containing snapshot metadata and body payload.
 *
 *  \details    Implements the receiver logic according to Section 7 of the
 *      Raft Paper.
 *
 *      Specification Invariants (Diego Ongaro Dissertation, Section 7, Table 13):
 *      1. Reply immediately if term < currentTerm.
 *      2. Create new snapshot file if it is the first chunk (this implementation
 *          assumes an atomic/single-chunk transfer).
 *      3. If existing log entry has same index and term as snapshot’s last
 *          included entry, retain log entries following it and reply.
 *      4. Discard the entire log.
 *      5. Reset state machine using snapshot contents.
 *
 */
void handle_request(context& ctx, term_t term, server_id_t src_id, const snapshot_message& msg);

/**
 *  \brief  Initiates sending a snapshot to a specific follower.
 *  \param  ctx - current server state context.
 *  \param  p_peer - target follower to which the snapshot is being sent.
 *
 *  \details    Raft Paper, Section 7 "Log compaction": "An entry is discarded
 *      from the log once it is committed and written to a snapshot... If a
 *      follower is so far behind that the next log entry the leader needs to
 *      send has already been discarded, the leader must send an InstallSnapshot
 *      RPC instead."
 *
 *      This method is invoked by the leader if a follower's log is so far behind
 *      that the log entries required for replication via AppendEntries RPC have
 *      already been discarded (compacted) from the leader's log.
 */
void request(context& ctx, const peer& p);

} // namespace snapshot
} // namespace details
} // namespace raft
} // namespace wstux

#endif /* _LIBS_RAFT_SNAPSHOT_HANDLER_H_ */
