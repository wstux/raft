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

#ifndef _TESTS_RAFT_EMPTY_IO_H_
#define _TESTS_RAFT_EMPTY_IO_H_

#include <algorithm>
#include <atomic>
#include <unordered_map>

#include "raft/io.h"
#include "raft/details/connection/messages.h"
#include "raft/details/connection/serialization.h"

namespace wstux {
namespace raft {
namespace tests {

class empty_client final
{
public:
    using ptr = std::shared_ptr<empty_client>;

public:
    void send(const buffer_type& b) { buffer = b; }

    static empty_client::ptr make() { return std::make_shared<empty_client>(); }

public:
    buffer_type buffer;
};

class empty_io final : public io
{
public:
    using ptr = std::shared_ptr<empty_io>;

public:
    empty_io() { cfg.scheduler_threads_count = 2; }
    virtual ~empty_io() {}

    virtual bool append(const entry::list& entrs) override final
    {
        if (is_append) {
            index_t i = start_index + entries.size();
            for (const entry::ptr& e : entrs) {
                entries.emplace(i++, e);
            }
        }
        return is_append;
    }

    virtual cluster_config bootstrap() const override final { return cluster_cfg; }
    virtual config configuration() const override final { return cfg; };

    virtual void deinit() override final {}

    virtual snapshot::ptr get_snapshot() const override final { return p_snapshot; }

    virtual bool init(server_id_t id) override final
    {
        if (clients.empty() && ! cluster_cfg.servers.empty()) {
            for (const server_config& cfg : cluster_cfg.servers) {
                if (cfg.id != id) {
                    clients.emplace(cfg.id, empty_client::make());
                }
            }
        }
        return is_init;
    }

    virtual entry::list load_entries() override final
    {
        entry::list entrs;
        entrs.reserve(entries.size());
        for (const std::map<index_t, entry::ptr>::value_type& e : entries) {
            entrs.push_back(e.second);
        }
        return entrs;
    }

    virtual index_t load_snapshot_index() override final { return snapshot_index; }
    virtual term_t load_snapshot_term() override final { return snapshot_term; }
    virtual index_t load_start_index() override final { return start_index; }
    virtual term_t load_term() override final { return 0; }
    virtual bool reconfigure(server_id_t) override final { return true; }
    virtual void send(server_id_t id, const std::string&, const buffer_type& msg) override final { clients.at(id)->send(msg); }

    virtual bool set_snapshot(snapshot::ptr p_sh) override final
    {
        p_snapshot = p_sh;
        return true;
    }

    virtual void set_term(term_t) override final {}
    virtual void set_voted_for(server_id_t) override final {}

    virtual bool truncate(const index_t begin) override final
    {
        if (is_truncate) {
            std::map<index_t, entry::ptr>::iterator it = entries.find(begin);
            if (it != entries.end()) {
                entries.erase(it, entries.end());
            }
        }
        return is_truncate;
    }

    virtual server_id_t voted_for() const override final { return gk_invalid_id; }

public:
    config cfg;
    cluster_config cluster_cfg;

    std::unordered_map<server_id_t, empty_client::ptr> clients;

    std::map<index_t, entry::ptr> entries;

    index_t snapshot_index = 0;
    term_t snapshot_term = 0;

    index_t start_index = 1;

    snapshot::ptr p_snapshot = nullptr;

    bool is_init = true;
    bool is_append = true;
    bool is_truncate = true;
    bool is_stop = false;
};

} // namespace tests
} // namespace raft
} // namespace wstux

#endif /* _TESTS_RAFT_EMPTY_IO_H_ */
