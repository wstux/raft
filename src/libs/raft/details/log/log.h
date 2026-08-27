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

#ifndef _LIBS_RAFT_LOG_LOG_H_
#define _LIBS_RAFT_LOG_LOG_H_

#include <map>

#include "raft/io.h"

namespace wstux {
namespace raft {
namespace details {
namespace log {

struct store final
{
    using entry_map = std::map<index_t, entry::ptr>;

    entry::list acquire(index_t begin_idx) const;

    void append(entry::ptr p_entry);

    void append_change(term_t term, const cluster_config& cfg);

    void append_command(term_t term, buffer_type buf);

    entry::ptr get_entry(index_t idx) const;

    index_t last_index() const;

    term_t last_term() const;

    void load(index_t snapshot_index, term_t snapshot_term, index_t start_index);

    void restore(index_t last_idx, term_t last_term);

    void take_snapshot(index_t new_last_index);

    term_t term(index_t idx) const;

    void truncate(index_t begin_idx);

    entry_map entries;
    index_t offset;
    struct
    {
        index_t last_index;
        term_t last_term;
    } snapshot;
};

} // namespace log
} // namespace details
} // namespace raft
} // namespace wstux

#endif /* _LIBS_RAFT_LOG_LOG_H_ */
