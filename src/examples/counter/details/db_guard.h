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

#ifndef _EXAMPLES_CLUSTER_DB_GUARD_H_
#define _EXAMPLES_CLUSTER_DB_GUARD_H_

#include <memory>
#include <string_view>

#include <lmdb.h>

namespace wstux {
namespace examples {
namespace cluster {
namespace details {

struct db_guard final
{
    using ptr = std::shared_ptr<db_guard>;

    static constexpr size_t k_map_size_dfl = 100 * 1024 * 1024; // 100 Mb

    db_guard()
        : p_env(nullptr)
    {}

    ~db_guard()
    {
        if (p_env) {
            mdb_env_close(p_env);
        }
    }

    db_guard(const db_guard&) = delete;
    db_guard& operator=(const db_guard&) = delete;

    bool create_env(const std::string& path, size_t map_size = k_map_size_dfl)
    {
        if (mdb_env_create(&p_env) != MDB_SUCCESS) {
            p_env = nullptr;
            return false;
        }
        // Create 3 separate DBs in one file.
        if (mdb_env_set_maxdbs(p_env, 3) != MDB_SUCCESS) {
            mdb_env_close(p_env);
            p_env = nullptr;
            return false;
        }
        if (mdb_env_set_mapsize(p_env, map_size) != MDB_SUCCESS) {
            mdb_env_close(p_env);
            p_env = nullptr;
            return false;
        }
        // Open with sub-db supporting (MDB_CREATE)
        if (mdb_env_open(p_env, path.c_str(), MDB_CREATE, 0664) != MDB_SUCCESS) {
            mdb_env_close(p_env);
            p_env = nullptr;
            return false;
        }
        return true;
    }

    MDB_dbi open_db(MDB_txn* p_txn, const std::string_view& db_name, unsigned int flags) const
    {
        MDB_dbi dbi = 0;
        if (mdb_dbi_open(p_txn, db_name.data(), flags, &dbi) != MDB_SUCCESS) {
            mdb_txn_abort(p_txn);
            return 0;
        }
        return dbi;
    }

    MDB_env* p_env;
};

} // namespace details
} // namespace cluster
} // namespace examples
} // namespace wstux

#endif /* _EXAMPLES_CLUSTER_DB_GUARD_H_ */
