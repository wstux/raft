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
#include <type_traits>

#include "raft/details/connection/serialization.h"
#include "raft/details/log/log.h"

namespace wstux {
namespace raft {
namespace details {
namespace {

template<typename TLogStore>
using iterator_type = typename std::conditional<std::is_const<TLogStore>::value,
    typename TLogStore::entry_buffer::const_iterator, typename TLogStore::entry_buffer::iterator>::type;

template<typename TLogStore>
iterator_type<TLogStore> find_entry(TLogStore* p_log, const index_t idx)
{
    TLogStore& l = *p_log;
    if (l.entries.size() == 0 || idx <= l.offset || idx > (l.offset + l.entries.size())) {
        return l.entries.end();
    }

    return l.entries.begin() + (idx - l.offset - 1);
}

} // <anonymous> namespace

entry::list log_store::acquire(index_t begin_idx) const
{
    assert(begin_idx > 0);

    entry::list ent_list;
    if (begin_idx <= offset || begin_idx > last_index()) {
        return ent_list;
    }

    entry_buffer::const_iterator it = find_entry(this, begin_idx);
    if (it == entries.cend()) {
        return ent_list;
    }

    ent_list.reserve(entries.end() - it);
    ent_list.assign(it, entries.end());

    return ent_list;
}

void log_store::append(entry::ptr p_entry)
{
    if (entries.full()) {
        entries.set_capacity((entries.capacity() + 1) * 2);
    }
    entries.push_back(std::move(p_entry));
}

void log_store::append_change(term_t term, const cluster_config& cfg)
{
    entry::ptr e = std::make_shared<entry>();
    e->term = term;
    e->type = entry_type::change;
    e->buffer = serialize<cluster_config>(cfg);

    append(std::move(e));
}

void log_store::append_command(term_t term, buffer_type buf)
{
    entry::ptr e = std::make_shared<entry>();
    e->term = term;
    e->type = entry_type::command;
    e->buffer = std::move(buf);

    append(std::move(e));
}

entry::ptr log_store::get_entry(index_t idx) const
{
    entry_buffer::const_iterator it = find_entry(this, idx);
    if (it == entries.cend()) {
        return nullptr;
    }
    return *it;
}

index_t log_store::last_index() const
{
    if (entries.size() == 0 && snapshot.last_index != 0) {
        assert(offset <= snapshot.last_index);
    }
    return offset + entries.size();
}

term_t log_store::last_term() const
{
    const index_t last_idx = last_index();
    return last_idx > 0 ? term(last_idx) : 0;
}

void log_store::load(index_t snapshot_index, term_t snapshot_term, index_t start_index)
{
    assert(entries.empty());
    assert(start_index > 0);
    assert(start_index <= snapshot_index + 1);
    assert(snapshot_index == 0 || snapshot_term != 0);

    snapshot.last_index = snapshot_index;
    snapshot.last_term = snapshot_term;
    offset = start_index - 1;
}

void log_store::restore(index_t last_idx, term_t last_term)
{
    entries.clear();
    snapshot.last_index = last_idx;
    snapshot.last_term = last_term;
    offset = last_idx;
}

void log_store::take_snapshot(index_t new_last_index, size_t trailing)
{
    const term_t new_last_term = term(new_last_index);
    assert(new_last_term != 0);

    snapshot.last_index = new_last_index;
    snapshot.last_term = new_last_term;

    // If log has not at least N entries preceeding the given last index, then there's nothing to remove
    if (new_last_index <= trailing || find_entry(this, new_last_index - trailing) == entries.cend()) {
        return;
    }

    if (new_last_index <= trailing) {
        return;
    }

    // Find a position to the element following (new_last_index - trailing)
    assert(new_last_index > trailing);
    const index_t retain_idx = new_last_index - static_cast<index_t>(trailing);
    assert(retain_idx > offset);
    const size_t remove_count = static_cast<size_t>(retain_idx - offset);
    // Delete everything from the beginning to position (not including position)
    if (remove_count >= entries.size()) {
        entries.clear();
        offset = new_last_index;
    } else {
        entries.erase(entries.begin(), entries.begin() + remove_count);
        offset = retain_idx;
    }
}

term_t log_store::term(index_t idx) const
{
    assert(idx > 0);
    assert(offset <= snapshot.last_index);

    if ((idx < offset + 1 && idx != snapshot.last_index) || idx > last_index()) {
        return 0;
    }

    entry_buffer::const_iterator it = find_entry(this, idx);
    if (idx == snapshot.last_index) {
        assert(snapshot.last_term != 0);
        if (it != entries.end()) {
            assert((*it)->term == snapshot.last_term);
        }
        return snapshot.last_term;
    }

    assert(it != entries.end());
    return (*it)->term;
}

void log_store::truncate(index_t begin_idx)
{
    assert(begin_idx > snapshot.last_index);

    if (entries.empty() || begin_idx > last_index()) {
        return;
    }

    if (begin_idx <= offset + 1) {
        entries.clear();
    } else {
        entry_buffer::iterator it = find_entry(this, begin_idx);
        entries.erase(it, entries.end());
    }
}

} // namespace details
} // namespace raft
} // namespace wstux
