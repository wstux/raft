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

#ifndef _LIBS_RAFT_HANDLERS_TIMEOUT_HANDLER_H_
#define _LIBS_RAFT_HANDLERS_TIMEOUT_HANDLER_H_

#include "raft/details/context.h"

namespace wstux {
namespace raft {
namespace details {
namespace timeout {

/**
 *  \brief  Cancels the currently scheduled election timeout task.
 *  \param  ctx - current server state context.
 *
 *  \details    Prevents the node from initiating new elections until the task
 *      is rescheduled.
 */
void election_cancel_task(context& ctx);

/**
 *  \brief  Reschedules the election timeout task with a new duration.
 *  \param  ctx - current server state context.
 *
 *  \details    Raft Paper, Section 5.2 (Leader election): "Raft uses randomized
 *      election timeouts to ensure that split votes are rare and that they are
 *      resolved quickly. To prevent split votes in the first place, election
 *      timeouts are chosen randomly from a fixed interval (e.g., 150–300ms)."
 */
void election_restart_task(context& ctx);

/**
 *  \brief  Main event handler triggered when the election timeout expires.
 *  \param  ctx - current server state context.
 *
 *  \details    Raft Paper, Section 5.2 (Leader election): "If a follower receives
 *      no communication over a period of time called the election timeout, then
 *      it assumes there is no viable leader and begins an election to choose a
 *      new leader."
 *
 *      The primary time-dependent state machine trigger. It forces inactive
 *      leaders to step down, handles the reset of the Pre-vote phase, and
 *      transitions passive followers into candidates.
 */
void election_timeout_task(context& ctx);

/**
 *  \brief  Cancels the currently scheduled heartbeat timeout task.
 *  \param  ctx - current server state context.
 *
 *  \details    Used primarily when a leader steps down or a node changes its role.
 */
void heartbeat_cancel_task(context& ctx);

/**
 *  \brief  Reschedules the heartbeat timeout task using the standard interval.
 *  \param  ctx - current server state context.
 *
 *  \details    Raft Paper, Section 5.2 (Leader election): "The leader sends
 *      periodic heartbeat messages (AppendEntries RPCs that carry no log entries)
 *      to all followers to maintain its authority."
 */
void heartbeat_restart_task(context& ctx);

/**
 *  \brief  Handler executed when the heartbeat interval expires.
 *  \param  ctx - current server state context.
 *
 *  \details If the current node is the leader, it broadcasts heartbeats. If it
 *      is a follower, it checks whether the current leader's lease has expired.
 */
void heartbeat_timeout_task(context& ctx);

} // namespace timeout
} // namespace details
} // namespace raft
} // namespace wstux

#endif /* _LIBS_RAFT_HANDLERS_TIMEOUT_HANDLER_H_ */
