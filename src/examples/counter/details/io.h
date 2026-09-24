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

#ifndef _EXAMPLES_RAFT_COUNTER_IO_H_
#define _EXAMPLES_RAFT_COUNTER_IO_H_

#include <cassert>
#include <algorithm>
#include <memory>
#include <mutex>
#include <unordered_map>

#include "raft/io.h"

#include "counter/config.h"
#include "counter/details/client.h"
#include "counter/details/logging.h"

namespace wstux {
namespace examples {
namespace counter {
namespace details {

class io final : public raft::io
{
public:
    using ptr = std::shared_ptr<io>;

public:
    io(const config::server_config::list& servers, raft::logging_handler::severity_level lvl)
        : m_servers(servers)
        , m_term(1)
        , m_voted_for(raft::gk_invalid_id)
        , m_level(lvl)
        , m_logger(m_level)
    {
        m_cfg.scheduler_threads_count = 4;
    }

    virtual ~io() {}

    virtual bool append(const raft::entry::list& entries) noexcept override final
    {
        std::lock_guard<std::mutex> lock(m_entries_mutex);
        m_entries.assign(entries.begin(), entries.end());
        return true;
    }

    virtual raft::config configuration() const noexcept override final { return m_cfg; };

    virtual void deinit() noexcept override final {}

    virtual std::optional<raft::snapshot> get_snapshot() const noexcept override final
    {
        std::lock_guard<std::mutex> lock(m_snapshot_mutex);
        return m_snapshot;
    }

    virtual bool init(raft::server_id_t id) noexcept override final
    {
        for (const config::server_config& cfg : m_servers) {
            std::unordered_map<raft::server_id_t, client::ptr>::iterator it = m_clients.find(cfg.id);
            assert(it == m_clients.end());
            if (cfg.id != id) {
                m_clients.emplace(cfg.id, std::make_shared<client>(cfg.endpoint, m_level));
            }
        }
        return true;
    }

    virtual raft::entry::list load_entries() noexcept override final
    {
        std::lock_guard<std::mutex> lock(m_entries_mutex);
        return m_entries;
    }

    virtual raft::index_t load_snapshot_index() noexcept override final
    {
        std::lock_guard<std::mutex> lock(m_snapshot_mutex);
        if (m_snapshot) {
            return m_snapshot->index;
        }
        return 0;
    }

    virtual raft::term_t load_snapshot_term() noexcept override final
    {
        std::lock_guard<std::mutex> lock(m_snapshot_mutex);
        if (m_snapshot) {
            return m_snapshot->term;
        }
        return 0;
    }

    virtual raft::index_t load_start_index() noexcept override final
    {
        std::lock_guard<std::mutex> lock(m_entries_mutex);
        return m_entries.size() + 1;
    }

    virtual raft::term_t load_term() noexcept override final { return m_term; }

    virtual bool reconfigure(raft::server_id_t) noexcept override final { return true; }

    virtual void send(raft::server_id_t id, std::string_view, const raft::buffer_type& msg) noexcept override final
    {
        m_clients.at(id)->send(msg);
    }

    virtual bool set_snapshot(const raft::snapshot& sh) noexcept override final
    {
        std::lock_guard<std::mutex> lock(m_snapshot_mutex);
        m_snapshot = sh;
        m_term = m_snapshot->term;

        std::lock_guard<std::mutex> entries_lock(m_entries_mutex);
        if (m_snapshot->index < m_entries.size()) {
            m_entries.erase(m_entries.begin() + m_snapshot->index, m_entries.end());
        } else if (m_snapshot->index >= m_entries.size()) {
            m_entries.resize(m_snapshot->index);
        }
        return true;
    }

    virtual void set_term(raft::term_t term) noexcept override final { m_term = term; }

    virtual void set_voted_for(raft::server_id_t id) noexcept override final { m_voted_for = id; }

    virtual bool truncate(const raft::index_t begin) noexcept override final
    {
        std::lock_guard<std::mutex> lock(m_entries_mutex);
        if (begin < m_entries.size()) {
            m_entries.erase(m_entries.begin() + begin, m_entries.end());
        }
        return true;
    }

    virtual raft::server_id_t voted_for() const noexcept override final { return m_voted_for; }

private:
    config::server_config::list m_servers;
    raft::config m_cfg;

    raft::term_t m_term;
    raft::server_id_t m_voted_for;

    std::unordered_map<raft::server_id_t, client::ptr> m_clients;

    std::mutex m_entries_mutex;
    raft::entry::list m_entries;

    mutable std::mutex m_snapshot_mutex;
    std::optional<raft::snapshot> m_snapshot;

    raft::logging_handler::severity_level m_level;
    logging_handler m_logger;
};

} // namespace details
} // namespace counter
} // namespace examples
} // namespace wstux

#endif /* _EXAMPLES_RAFT_COUNTER_IO_H_ */
