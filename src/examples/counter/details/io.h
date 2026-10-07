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

#ifndef _EXAMPLES_CLUSTER_IO_H_
#define _EXAMPLES_CLUSTER_IO_H_

#include <memory>
#include <mutex>

#include <lmdb.h>

#include <raft/io.h>

#include "counter/details/client.h"
#include "counter/details/db_guard.h"

namespace wstux {
namespace examples {
namespace cluster {
namespace details {

class io : public raft::io
{
public:
    using ptr = std::shared_ptr<io>;

public:
    io(db_guard::ptr p_db_guard, raft::logging_handler::severity_level lvl);

    virtual ~io() {}

    virtual bool append(const raft::entry::list& entries) noexcept override final;

    virtual void deinit() noexcept override final;

    virtual std::optional<raft::snapshot> get_snapshot() const noexcept override final;

    virtual bool init(raft::server_id_t id) noexcept override final;

    virtual raft::entry::list load_entries() noexcept override final;

    virtual raft::index_t load_snapshot_index() noexcept override final;

    virtual raft::term_t load_snapshot_term() noexcept override final;

    virtual raft::index_t load_start_index() noexcept override final;

    virtual raft::term_t load_term() noexcept override final;

    virtual bool reconfigure(raft::server_id_t) noexcept override final { return false; }

    virtual void send(raft::server_id_t id, std::string_view endpoint, const raft::buffer_type& msg) noexcept override final;

    virtual bool set_snapshot(const raft::snapshot& sh) noexcept override final;

    virtual void set_term(raft::term_t term) noexcept override final;

    virtual void set_voted_for(raft::server_id_t id) noexcept override final;

    virtual bool truncate(const raft::index_t begin) noexcept override final;

    virtual raft::server_id_t voted_for() const noexcept override final;

private:
    MDB_txn* begin_transaction() const noexcept
    {
        MDB_txn* p_txn = nullptr;
        if (mdb_txn_begin(m_p_db_guard->p_env, nullptr, 0, &p_txn) != MDB_SUCCESS) {
            return nullptr;
        }
        return p_txn;
    }

    bool commit_transaction(MDB_txn* p_txn) const noexcept
    {
        return (mdb_txn_commit(p_txn) == MDB_SUCCESS);
    }

    raft::index_t last_log_index(MDB_txn* p_txn) const noexcept
    {
        MDB_cursor* p_cursor = nullptr;
        if (mdb_cursor_open(p_txn, m_logs_dbi, &p_cursor) != MDB_SUCCESS) {
            return 0;
        }
        MDB_val key;
        MDB_val value;
        raft::index_t last_idx = 0;
        if (mdb_cursor_get(p_cursor, &key, &value, MDB_LAST) == MDB_SUCCESS) {
            std::memcpy(&last_idx, key.mv_data, sizeof(raft::index_t));
        }
        mdb_cursor_close(p_cursor);
        return last_idx;
    }

private:
    mutable std::recursive_mutex m_mutex;

    db_guard::ptr m_p_db_guard;
    raft::server_id_t m_id = raft::gk_invalid_id;
    MDB_dbi m_meta_dbi = 0;
    MDB_dbi m_logs_dbi = 0;

    std::mutex m_clients_mutex;
    std::unordered_map<raft::server_id_t, client::ptr> m_clients;

    raft::logging_handler::severity_level m_level;
};

} // namespace details
} // namespace cluster
} // namespace examples
} // namespace wstux

#endif /* _EXAMPLES_CLUSTER_IO_H_ */
