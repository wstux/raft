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

#ifndef _EXAMPLES_CLUSTER_FSM_H_
#define _EXAMPLES_CLUSTER_FSM_H_

#include <memory>
#include <mutex>

#include <raft/io.h>

#include "counter/details/db_guard.h"

namespace wstux {
namespace examples {
namespace cluster {
namespace details {

class fsm final : public raft::fsm
{
public:
    using ptr = std::shared_ptr<fsm>;

public:
    explicit fsm(db_guard::ptr p_db_guard)
        : m_p_db_guard(std::move(p_db_guard))
    {
        init();
    }

    virtual ~fsm() {}

    virtual bool apply(const raft::buffer_type& buf) noexcept override final
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (! init()) {
            return false;
        }

        MDB_txn* p_txn = nullptr;
        if (mdb_txn_begin(m_p_db_guard->p_env, nullptr, 0, &p_txn) != MDB_SUCCESS) {
            return false;
        }

        MDB_val key{ 5, const_cast<char*>("state") };
        MDB_val val{ buf.size(), const_cast<char*>(buf.data()) };

        if (mdb_put(p_txn, m_fsm_dbi, &key, &val, 0) != MDB_SUCCESS) {
            mdb_txn_abort(p_txn);
            return false;
        }

        m_buffer = buf;
        return mdb_txn_commit(p_txn) == MDB_SUCCESS;
    }

    virtual bool restore(const raft::buffer_type& buf) noexcept override final { return apply(buf); }

    virtual bool take_snapshot(raft::buffer_type& buf) noexcept override final
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        buf = m_buffer;
        return true;
    }

    raft::buffer_type get_buffer() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_buffer;
    }

private:
    bool init()
    {
        if (is_inited()) {
            return true;
        }

        if (! m_p_db_guard) {
            return false;
        }

        MDB_txn* p_txn = nullptr;
        if (mdb_txn_begin(m_p_db_guard->p_env, nullptr, 0, &p_txn) != MDB_SUCCESS) {
            return false;
        }

        if (mdb_dbi_open(p_txn, "fsm", MDB_CREATE, &m_fsm_dbi) != MDB_SUCCESS) {
            mdb_txn_abort(p_txn);
            return false;
        }

        if (mdb_txn_commit(p_txn) != MDB_SUCCESS) {
            return false;
        }

        p_txn = nullptr;
        if (mdb_txn_begin(m_p_db_guard->p_env, nullptr, MDB_RDONLY, &p_txn) != MDB_SUCCESS) {
            return false;
        }

        MDB_val key{ 5, const_cast<char*>("state") };
        MDB_val value;

        if (mdb_get(p_txn, m_fsm_dbi, &key, &value) != MDB_SUCCESS) {
            mdb_txn_abort(p_txn);
            return true;
        }

        const char* src = static_cast<const char*>(value.mv_data);
        m_buffer = raft::buffer_type(src, src + value.mv_size);

        mdb_txn_abort(p_txn);
        return true;
    }

    bool is_inited() const { return m_fsm_dbi != 0; }

private:
    db_guard::ptr m_p_db_guard;
    MDB_dbi m_fsm_dbi = 0;

    mutable std::mutex m_mutex;
    raft::buffer_type m_buffer;
};

} // namespace details
} // namespace cluster
} // namespace examples
} // namespace wstux

#endif /* _EXAMPLES_CLUSTER_FSM_H_ */
