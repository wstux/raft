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

#include <cstdint>
#include <cstring>

#include "counter/details/io.h"

namespace wstux {
namespace examples {
namespace cluster {
namespace details {
namespace {

template<typename T> T deserialize(const char* ptr);
template<typename T> raft::buffer_type serialize(const T& val);

template<typename T> T read(const char*& ptr);
template<typename T> void write(raft::buffer_type& buf, const T& val);

template<> uint32_t read<uint32_t>(const char*& ptr)
{
    uint32_t val;
    std::memcpy(&val, ptr, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    return val;
}

template<> void write<uint32_t>(raft::buffer_type& buf, const uint32_t& val)
{
    buf.reserve(buf.size() + sizeof(uint32_t));
    const char* ptr = reinterpret_cast<const char*>(&val);
    buf.insert(buf.end(), ptr, ptr + sizeof(uint32_t));
}

template<> uint64_t read<uint64_t>(const char*& ptr)
{
    uint64_t val;
    std::memcpy(&val, ptr, sizeof(uint64_t));
    ptr += sizeof(uint64_t);
    return val;
}

template<> void write<uint64_t>(raft::buffer_type& buf, const uint64_t& val)
{
    buf.reserve(buf.size() + sizeof(uint64_t));
    const char* ptr = reinterpret_cast<const char*>(&val);
    buf.insert(buf.end(), ptr, ptr + sizeof(uint64_t));
}

template<> std::string read<std::string>(const char*& ptr)
{
    uint32_t size = read<uint32_t>(ptr);
    std::string str(ptr, size);
    ptr += size;
    return str;
}

template<> void write<std::string>(raft::buffer_type& buf, const std::string& str)
{
    buf.reserve(buf.size() + sizeof(uint32_t) + str.size() * sizeof(std::string::value_type));
    write<uint32_t>(buf, static_cast<uint32_t>(str.size()));
    buf.insert(buf.end(), str.begin(), str.end());
}

template<> raft::buffer_type read<raft::buffer_type>(const char*& ptr)
{
    uint32_t size = read<uint32_t>(ptr);
    raft::buffer_type buf(ptr, ptr + size);
    ptr += size;
    return buf;
}

template<> void write<raft::buffer_type>(raft::buffer_type& buf, const raft::buffer_type& data)
{
    buf.reserve(buf.size() + sizeof(uint32_t) + data.size() * sizeof(raft::buffer_type::value_type));
    write<uint32_t>(buf, static_cast<uint32_t>(data.size()));
    buf.insert(buf.end(), data.begin(), data.end());
}

template<> raft::entry::ptr deserialize<raft::entry::ptr>(const char* ptr)
{
    raft::entry::ptr e = std::make_shared<raft::entry>();
    e->term = read<uint32_t>(ptr);
    e->type = static_cast<raft::entry_type>(read<uint32_t>(ptr));
    e->buffer = read<raft::buffer_type>(ptr);
    return e;
}

template<> raft::buffer_type serialize<raft::entry::ptr>(const raft::entry::ptr& p_entry)
{
    raft::buffer_type buf;
    write<uint32_t>(buf, p_entry->term);
    write<uint32_t>(buf, static_cast<uint32_t>(p_entry->type));
    write<raft::buffer_type>(buf, p_entry->buffer);
    return buf;
}

template<> raft::snapshot deserialize<raft::snapshot>(const char* ptr)
{
    raft::snapshot sh;
    sh.index = read<uint32_t>(ptr);
    sh.term = read<uint32_t>(ptr);
    sh.conf_index = read<uint32_t>(ptr);
    uint32_t server_count = read<uint32_t>(ptr);
    sh.conf.servers.reserve(server_count);
    for (uint32_t i = 0; i < server_count; ++i) {
        raft::server_config s;
        s.id = read<uint64_t>(ptr);
        s.address = read<std::string>(ptr);
        s.is_voter = (*ptr++ != 0);
        sh.conf.servers.push_back(std::move(s));
    }
    sh.buffer = read<raft::buffer_type>(ptr);
    return sh;
}

template<> raft::buffer_type serialize<raft::snapshot>(const raft::snapshot& sh)
{
    raft::buffer_type buf;
    write<uint32_t>(buf, sh.index);
    write<uint32_t>(buf, sh.term);
    write<uint32_t>(buf, sh.conf_index);
    write<uint32_t>(buf, static_cast<uint32_t>(sh.conf.servers.size()));
    for (const raft::server_config& s : sh.conf.servers) {
        write<uint64_t>(buf, s.id);
        write<std::string>(buf, s.address);
        buf.push_back(s.is_voter ? 1 : 0);
    }
    write<raft::buffer_type>(buf, sh.buffer);
    return buf;
}

} // <anonymous> namespace

io::io(db_guard::ptr p_db_guard, raft::logging_handler::severity_level lvl)
    : m_p_db_guard(std::move(p_db_guard))
    , m_level(lvl)
{}

bool io::append(const raft::entry::list& entries) noexcept
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    MDB_txn* p_txn = begin_transaction();
    if (p_txn == nullptr) {
        return false;
    }

    raft::index_t next_idx = last_log_index(p_txn) + 1;
    for (const raft::entry::ptr& e : entries) {
        raft::buffer_type buf = serialize<raft::entry::ptr>(e);

        MDB_val key;
        key.mv_size = sizeof(raft::index_t);
        key.mv_data = &next_idx;

        MDB_val value;
        value.mv_size = buf.size();
        value.mv_data = buf.data();

        if (mdb_put(p_txn, m_logs_dbi, &key, &value, 0) != MDB_SUCCESS) {
            mdb_txn_abort(p_txn);
            return false;
        }
        ++next_idx;
    }

    if (! commit_transaction(p_txn)) {
        return false;
    }
    return true;
}

void io::deinit() noexcept
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    if (m_id != raft::gk_invalid_id) {
        m_meta_dbi = 0;
        m_logs_dbi = 0;
        m_id = raft::gk_invalid_id;
    }
}

std::optional<raft::snapshot> io::get_snapshot() const noexcept
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    MDB_txn* p_txn = begin_transaction();
    if (p_txn == nullptr) {
        return std::nullopt;
    }

    MDB_val key;
    MDB_val value;
    key.mv_size = 4;
    key.mv_data = const_cast<char*>("snap");

    if (mdb_get(p_txn, m_meta_dbi, &key, &value) != MDB_SUCCESS) {
        mdb_txn_abort(p_txn);
        return std::nullopt;
    }

    raft::snapshot sh = deserialize<raft::snapshot>(static_cast<const char*>(value.mv_data));
    mdb_txn_abort(p_txn);
    return sh;
}

bool io::init(raft::server_id_t id) noexcept
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    if (! m_p_db_guard) {
        return false;
    }

    MDB_txn* p_txn = begin_transaction();
    if (p_txn == nullptr) {
        return false;
    }

    m_meta_dbi = m_p_db_guard->open_db(p_txn, "meta", MDB_CREATE);
    if (m_meta_dbi == 0) {
        return false;
    }
    m_logs_dbi = m_p_db_guard->open_db(p_txn, "logs", MDB_CREATE | MDB_INTEGERKEY);
    if (m_logs_dbi == 0) {
        return false;
    }

    if (! commit_transaction(p_txn)) {
        return false;
    }

    m_id = id;
    return true;
}

raft::entry::list io::load_entries() noexcept
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    raft::entry::list entries;

    MDB_txn* p_txn = begin_transaction();
    if (p_txn == nullptr) {
        return entries;
    }

    MDB_cursor* p_cursor = nullptr;
    if (mdb_cursor_open(p_txn, m_logs_dbi, &p_cursor) != MDB_SUCCESS) {
        mdb_txn_abort(p_txn);
        return entries;
    }

    MDB_val key;
    MDB_val value;
    int rc = mdb_cursor_get(p_cursor, &key, &value, MDB_FIRST);
    while (rc == MDB_SUCCESS) {
        entries.push_back(deserialize<raft::entry::ptr>(static_cast<const char*>(value.mv_data)));
        rc = mdb_cursor_get(p_cursor, &key, &value, MDB_NEXT);
    }

    mdb_cursor_close(p_cursor);
    mdb_txn_abort(p_txn);
    return entries;
}

raft::index_t io::load_snapshot_index() noexcept
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::optional<raft::snapshot> sh = get_snapshot();
    return sh ? sh->index : 0;
}

raft::term_t io::load_snapshot_term() noexcept
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::optional<raft::snapshot> sh = get_snapshot();
    return sh ? sh->term : 0;
}

raft::index_t io::load_start_index() noexcept
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    raft::index_t start_idx = 1;
    MDB_txn* p_txn = begin_transaction();
    if (p_txn == nullptr) {
        return start_idx;
    }
    MDB_cursor* p_cursor = nullptr;
    if (mdb_cursor_open(p_txn, m_logs_dbi, &p_cursor) != MDB_SUCCESS) {
        mdb_txn_abort(p_txn);
        return start_idx;
    }
    MDB_val key;
    MDB_val value;
    const int rc = mdb_cursor_get(p_cursor, &key, &value, MDB_FIRST);
    if (rc == MDB_SUCCESS) {
        std::memcpy(&start_idx, key.mv_data, sizeof(raft::index_t));
        mdb_cursor_close(p_cursor);
        mdb_txn_abort(p_txn);
    } else {
        mdb_cursor_close(p_cursor);
        mdb_txn_abort(p_txn);
        start_idx = load_snapshot_index() + 1;
    }
    return start_idx;
}

raft::term_t io::load_term() noexcept
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    MDB_txn* p_txn = begin_transaction();
    if (p_txn == nullptr) {
        return 1;
    }
    MDB_val key;
    MDB_val value;
    key.mv_size = 4;
    key.mv_data = const_cast<char*>("term");

    raft::term_t term = 1;
    if (mdb_get(p_txn, m_meta_dbi, &key, &value) == MDB_SUCCESS) {
        std::memcpy(&term, value.mv_data, sizeof(raft::term_t));
    }
    mdb_txn_abort(p_txn);
    return term;
}

void io::send(raft::server_id_t id, std::string_view endpoint, const raft::buffer_type& msg) noexcept
{
    client* p_client = nullptr;
    {
        std::unique_lock<std::mutex> lock(m_clients_mutex);
        std::unordered_map<raft::server_id_t, client::ptr>::iterator it = m_clients.find(id);
        if (it != m_clients.end()) {
            p_client = it->second.get();
        } else {
            p_client = m_clients.emplace(id, std::make_shared<client>(std::string(endpoint), m_level)).first->second.get();
        }
    }
    p_client->send(msg);
}

bool io::set_snapshot(const raft::snapshot& sh) noexcept
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    MDB_txn* p_txn = begin_transaction();
    if (p_txn == nullptr) {
        return false;
    }

    raft::buffer_type buf = serialize<raft::snapshot>(sh);
    MDB_val key;
    MDB_val value;
    key.mv_size = 4;
    key.mv_data = const_cast<char*>("snap");
    value.mv_size = buf.size();
    value.mv_data = buf.data();

    if (mdb_put(p_txn, m_meta_dbi, &key, &value, 0) != MDB_SUCCESS) {
        mdb_txn_abort(p_txn);
        return false;
    }
    if (! commit_transaction(p_txn)) {
        return false;
    }
    return true;
}

void io::set_term(raft::term_t term) noexcept
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    MDB_txn* p_txn = begin_transaction();
    if (p_txn == nullptr) {
        return;
    }

    MDB_val key;
    MDB_val value;
    key.mv_size = 4;
    key.mv_data = const_cast<char*>("term");
    value.mv_size = sizeof(raft::term_t);
    value.mv_data = &term;

    if (mdb_put(p_txn, m_meta_dbi, &key, &value, 0) == MDB_SUCCESS) {
        mdb_txn_commit(p_txn);
    } else {
        mdb_txn_abort(p_txn);
    }
}

void io::set_voted_for(raft::server_id_t id) noexcept
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    MDB_txn* p_txn = begin_transaction();
    if (p_txn == nullptr) {
        return;
    }

    MDB_val key;
    MDB_val value;
    key.mv_size = 5;
    key.mv_data = const_cast<char*>("voted");
    value.mv_size = sizeof(raft::server_id_t);
    value.mv_data = &id;

    if (mdb_put(p_txn, m_meta_dbi, &key, &value, 0) == MDB_SUCCESS) {
        mdb_txn_commit(p_txn);
    } else {
        mdb_txn_abort(p_txn);
    }
}

bool io::truncate(const raft::index_t begin) noexcept
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    MDB_txn* p_txn = begin_transaction();
    if (p_txn == nullptr) {
        return false;
    }

    MDB_cursor* p_cursor = nullptr;
    if (mdb_cursor_open(p_txn, m_logs_dbi, &p_cursor) != MDB_SUCCESS) {
        mdb_txn_abort(p_txn);
        return false;
    }

    MDB_val key;
    MDB_val value;
    raft::index_t begin_idx = begin;
    key.mv_size = sizeof(raft::index_t);
    key.mv_data = &begin_idx;

    int rc = mdb_cursor_get(p_cursor, &key, &value, MDB_SET_RANGE);
    while (rc == MDB_SUCCESS) {
        if (mdb_cursor_del(p_cursor, 0) != MDB_SUCCESS) {
            mdb_cursor_close(p_cursor);
            mdb_txn_abort(p_txn);
            return false;
        }
        rc = mdb_cursor_get(p_cursor, &key, &value, MDB_NEXT);
    }

    mdb_cursor_close(p_cursor);
    if (mdb_txn_commit(p_txn) != MDB_SUCCESS) {
        return false;
    }
    return true;
}

raft::server_id_t io::voted_for() const noexcept
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    MDB_txn* p_txn = begin_transaction();
    if (p_txn == nullptr) {
        return raft::gk_invalid_id;
    }

    MDB_val key;
    MDB_val value;
    key.mv_size = 5;
    key.mv_data = const_cast<char*>("voted");

    raft::server_id_t id = raft::gk_invalid_id;
    if (mdb_get(p_txn, m_meta_dbi, &key, &value) == MDB_SUCCESS) {
        std::memcpy(&id, value.mv_data, sizeof(raft::server_id_t));
    }

    mdb_txn_abort(p_txn);
    return id;
}

} // namespace details
} // namespace cluster
} // namespace examples
} // namespace wstux
