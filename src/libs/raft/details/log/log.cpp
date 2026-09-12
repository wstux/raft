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

#include <cassert>
#include <algorithm>

#include "raft/details/connection/serialization.h"
#include "raft/details/log/log.h"

namespace wstux {
namespace raft {
namespace details {
namespace log {

entry::list store::acquire(index_t begin_idx) const
{
    assert(begin_idx > 0);

    entry::list ent_list;
    entry_map::const_iterator it = entries.find(begin_idx);
    std::transform(it, entries.cend(), std::back_inserter(ent_list),
                   [](const entry_map::value_type& v) -> entry::ptr { return v.second; });
    return ent_list;
}

void store::append(entry::ptr p_entry)
{
    const index_t index = last_index() + 1;
    entries.emplace(index, std::move(p_entry));
}

void store::append_change(term_t term, const cluster_config& cfg)
{
    entry::ptr e = std::make_shared<entry>();
    e->term = term;
    e->type = entry_type::change;
    e->buffer = serialize<cluster_config>(cfg);

    append(std::move(e));
}

void store::append_command(term_t term, buffer_type buf)
{
    entry::ptr e = std::make_shared<entry>();
    e->term = term;
    e->type = entry_type::command;
    e->buffer = std::move(buf);

    append(std::move(e));
}

entry::ptr store::get_entry(index_t idx) const
{
    entry_map::const_iterator it = entries.find(idx);
    if (it == entries.cend()) {
        return nullptr;
    }

    return it->second;
}

index_t store::last_index() const
{
    if (entries.size() == 0 && snapshot.last_index != 0) {
        assert(offset <= snapshot.last_index);
    }
    return offset + entries.size();
}

term_t store::last_term() const
{
    const index_t last_idx = last_index();
    return last_idx > 0 ? term(last_idx) : 0;
}

void store::load(index_t snapshot_index, term_t snapshot_term, index_t start_index)
{
    assert(entries.size() == 0);
    assert(start_index > 0);
    assert(start_index <= snapshot_index + 1);
    assert(snapshot_index == 0 || snapshot_term != 0);

    snapshot.last_index = snapshot_index;
    snapshot.last_term = snapshot_term;
    offset = start_index - 1;
}

void store::restore(index_t last_idx, term_t last_term)
{
    assert(last_idx > 0);
    assert(last_term > 0);

    if (! entries.empty()) {
        index_t idx = entries.begin()->first;
        truncate(idx);
    }

    snapshot.last_index = last_idx;
    snapshot.last_term = last_term;
    offset = last_idx;
}

void store::take_snapshot(index_t new_last_index, size_t trailing)
{
    term_t new_last_term = term(new_last_index);
    assert(new_last_term != 0);

    snapshot.last_index = new_last_index;
    snapshot.last_term = new_last_term;

    // If log has not at least N entries preceeding the given last index, then there's nothing to remove
    if (new_last_index <= trailing || entries.find(new_last_index - trailing) == entries.cend()) {
        return;
    }

    // Find an iterator to the element following (new_last_index - trailing)
    entry_map::iterator it = entries.upper_bound(new_last_index - trailing);
    // Delete everything from the beginning to it (not including it)
    entries.erase(entries.begin(), it);

    // Adjust the offset since the first element in entries has changed
    offset = (! entries.empty()) ? entries.begin()->first - 1 : new_last_index;
}

term_t store::term(index_t idx) const
{
    assert(idx > 0);
    assert(offset <= snapshot.last_index);

    if ((idx < offset + 1 && idx != snapshot.last_index) || idx > last_index()) {
        return 0;
    }

    if (idx == snapshot.last_index) {
        assert(snapshot.last_term != 0);

        entry_map::const_iterator it = entries.find(idx);
        if (it != entries.cend()) {
            assert(it->second->term == snapshot.last_term);
        }
        return snapshot.last_term;
    }

    entry_map::const_iterator it = entries.find(idx);
    if (it != entries.cend()) {
        return it->second->term;
    }
    return 0;
}

void store::truncate(index_t begin_idx)
{
    //entry_map::iterator it = entries.find(begin_idx);
    //entries.erase(it, entries.end());
    if (entries.empty()) {
        return;
    }

    assert(begin_idx > snapshot.last_index);

    entry_map::iterator it = entries.find(begin_idx);
    assert(it != entries.cend());
    entries.erase(it, entries.end());
}

} // namespace log
} // namespace details
} // namespace raft
} // namespace wstux
