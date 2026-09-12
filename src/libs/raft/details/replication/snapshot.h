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

#ifndef _LIBS_RAFT_REPLICATION_SNAPSHOT_H_
#define _LIBS_RAFT_REPLICATION_SNAPSHOT_H_

#include "raft/io.h"
#include "raft/details/context.h"

namespace wstux {
namespace raft {
namespace details {
namespace replication {
namespace snapshot {
namespace async {

struct install_context final
{
    using ptr = std::shared_ptr<install_context>;

    raft::snapshot snapshot;
    index_t last_log_index;
    term_t term;
};

} // namespace async

bool install(context& ctx, index_t last_index, term_t last_term, cluster_config conf,
             index_t conf_index, buffer_type buffer, async::install_context::ptr& p_async_ctx);

bool install_callback(context& ctx, bool accept, raft::snapshot& snapshot);

bool restore(context& ctx, raft::snapshot& snapshot);

bool should_take_snapshot(context& ctx);

bool take_snapshot(context& ctx);

} // namespace snapshot
} // namespace replication
} // namespace details
} // namespace raft
} // namespace wstux

#endif /* _LIBS_RAFT_REPLICATION_SNAPSHOT_H_ */
