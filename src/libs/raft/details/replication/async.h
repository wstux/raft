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

#ifndef _LIBS_RAFT_REPLICATION_ASYNC_H_
#define _LIBS_RAFT_REPLICATION_ASYNC_H_

#include <memory>

#include "raft/io.h"
#include "raft/details/context.h"

namespace wstux {
namespace raft {
namespace details {
namespace replication {
namespace entries {
namespace async {

struct append_context final
{
    using ptr = std::shared_ptr<append_context>;

    term_t term;
    index_t index;
    index_t leader_commit;
    index_t last_stored;
    index_t last_index;
    entry::list entries;
};

struct apply_context final
{
    using ptr = std::shared_ptr<apply_context>;

    index_t index;
    entry::list entries;
};

} // namespace async
} // namespace entries
} // namespace replication
} // namespace details
} // namespace raft
} // namespace wstux

#endif /* _LIBS_RAFT_REPLICATION_ASYNC_H_ */
