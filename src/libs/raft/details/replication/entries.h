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

#ifndef _LIBS_RAFT_REPLICATION_ENTRIES_H_
#define _LIBS_RAFT_REPLICATION_ENTRIES_H_

#include <memory>

#include "raft/io.h"
#include "raft/details/context.h"
#include "raft/details/replication/async.h"

namespace wstux {
namespace raft {
namespace details {
namespace replication {
namespace entries {

bool append(context& ctx, term_t term, index_t leader_commit, index_t prev_log_index, term_t prev_log_term,
            const entry::list& entries, async::append_context::ptr& p_async_ctx);

bool append_callback(context& ctx, bool accept, term_t term, index_t index, index_t leader_commit, const entry::list& entries);

bool commit(context& ctx);

/**
 *  \brief  Advances the leader's commitIndex based on the matchIndex of the
 *      majority of nodes.
 *  \param  ctx - current server state context.
 *  \param  index - the index up to which the commit is proposed to be advanced.
 *
 *  \details Raft Paper, Section 5.3, 5.4: Implements the leader safety rules
 *      described in "Rules for Servers - Leaders": "If there exists an N such
 *      that N > commitIndex, a majority of matchIndex[i] ≥ N, and
 *      log[N].term == currentTerm: set commitIndex = N (5.3, 5.4)."
 */
void update_commit_index(context& ctx, const index_t index);

} // namespace entries
} // namespace replication
} // namespace details
} // namespace raft
} // namespace wstux

#endif /* _LIBS_RAFT_REPLICATION_ENTRIES_H_ */
