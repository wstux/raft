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
        , m_term(0)
        , m_voted_for(raft::gk_invalid_id)
        , m_level(lvl)
        , m_logger(m_level)
    {
        m_cfg.scheduler_threads_count = 4;
    }

    virtual ~io() {}

    virtual raft::cluster_config bootstrap() const override final
    {
        raft::cluster_config cluster_cfg;
        std::transform(m_servers.cbegin(), m_servers.cend(), std::back_inserter(cluster_cfg.servers),
            [](const config::server_config& cfg) -> raft::server_config {
                return raft::server_config(cfg.id, cfg.is_voter);
            });
        return cluster_cfg;
    }

    virtual raft::config configuration() const override final { return m_cfg; };

    virtual void deinit() override final {}

    virtual bool init(raft::server_id_t id) override final
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

    virtual raft::term_t load_term() override final { return m_term; }

    virtual bool reconfigure(raft::server_id_t) override final { return true; }

    virtual void send(raft::server_id_t id, const raft::buffer_type& msg) override final { m_clients.at(id)->send(msg); }

    virtual void set_term(raft::term_t term) override final { m_term = term; }

    virtual void set_voted_for(raft::server_id_t id) override final { m_voted_for = id; }

    virtual raft::server_id_t voted_for() const override final { return m_voted_for; }

    void update_counter(const uint64_t counter)
    {
        for (std::unordered_map<raft::server_id_t, client::ptr>::value_type& v : m_clients) {
            v.second->send_counter(counter);
        }
    }

private:
    config::server_config::list m_servers;
    raft::config m_cfg;

    raft::term_t m_term;
    raft::server_id_t m_voted_for;

    std::unordered_map<raft::server_id_t, client::ptr> m_clients;

    raft::logging_handler::severity_level m_level;
    logging_handler m_logger;
};

} // namespace details
} // namespace counter
} // namespace examples
} // namespace wstux

#endif /* _EXAMPLES_RAFT_COUNTER_IO_H_ */
