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

#ifndef _LIBS_RAFT_HANDLERS_VOTE_HANDLER_H_
#define _LIBS_RAFT_HANDLERS_VOTE_HANDLER_H_

#include "raft/details/context.h"
#include "raft/details/connection/messages.h"

namespace wstux {
namespace raft {
namespace details {
namespace vote {

/**
 *  \brief  Handler for incoming vote requests (both vote and pre-vote).
 *  \param  ctx - current server state context.
 *  \param  term - the term of the candidate that sent the request.
 *  \param  src_id - the id of the candidate server that sent the request.
 *  \param  msg - message.
 *
 *  \details    Implements the processing of incoming voting requests
 *      (Raft Paper, Figure 2 - RequestVote RPC).
 *      Handler processes incoming voting requests from candidates. It evaluates
 *      candidate logs, verifies terms, and maintains safety invariants for both
 *      the speculative pre-vote phase and the formal election phase.
 *
 *  \see    Raft Dissertation, Section 5.1 (Terms) - Rules for processing Terms.
 *  \see    Raft Dissertation, Figure 5.1 (Rules for Servers - All Servers) -
 *      Response to T > currentTerm.
 *  \see    Raft Dissertation, Section 9.6 (Preventing disruptions when a server
 *      rejoins a cluster) - Pre-Vote phase specification.
 */
void handle_request(context& ctx, term_t term, server_id_t src_id, const vote_message& msg);

/**
 *  \brief  Handler for responses to voting messages (pre-vote response/vote
 *      response).
 *  \param  ctx - current server state context.
 *  \param  term - the term in which this response was generated.
 *  \param  src_id - the id of the server that sent the message.
 *  \param  msg - message.
 *
 *  \details    Counts votes. Based on the results, decides whether to transition
 *      to Leader status or launch full elections (in the case of the Pre-Vote
 *      phase).
 *      This function processes responses from cluster peers following an election
 *      request. It tracks vote accumulation, verifies term invariants, and handles
 *      state transitions from Pre-Candidate to Candidate, or Candidate to Leader.
 */
void handle_response(context& ctx, term_t term, server_id_t src_id, const vote_response_message& msg);

/**
 *  \brief  Initiates the voting procedure from the current server side.
 *  \param  ctx - current server state context.
 *
 *  \details The function is called at the start of an election (Raft Paper,
 *      Section 5.2: "To begin an election, a follower...").
 *      This function builds and broadcasts election RPC payloads to all active
 *      cluster nodes. It identifies the current server's latest log parameters
 *      to satisfy the Raft election restriction invariant, ensuring that entries
 *      from older terms cannot overwrite newer ones.
 *      Triggered automatically upon an Election Timeout.
 */
void request(context& ctx);

} // namespace vote
} // namespace details
} // namespace raft
} // namespace wstux

#endif /* _LIBS_RAFT_HANDLERS_VOTE_HANDLER_H_ */
