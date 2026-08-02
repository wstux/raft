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

#include <map>
#include <memory>
#include <mutex>

#include "raft_le/io.h"

#include "counter/details/client.h"
#include "counter/details/logging.h"

namespace wstux {
namespace examples {
namespace counter {
namespace details {

class io final : public raft::le::io
{
public:
    using ptr = std::shared_ptr<io>;

public:
    io(const raft::le::cluster_config& p_cluster_cfg, raft::le::ilogger_factory::ptr p_factory)
        : m_cluster_cfg(p_cluster_cfg)
        , m_term(0)
        , m_voted_for(raft::le::gk_invalid_id)
        , m_p_factory(p_factory)
        , m_logger(p_factory->get_logger("Counter::Io"))
    {
        m_cfg.scheduler_threads_count = 4;
    }

    virtual ~io() {}

    virtual raft::le::cluster_config bootstrap() const override final { return m_cluster_cfg; }

    virtual raft::le::config configuration() const override final { return m_cfg; };

    virtual raft::le::iclient::ptr create_client(raft::le::server_id_t id, const std::string& endpoint) const override final
    {
        client::ptr p_client;

        std::map<raft::le::server_id_t, client::ptr>::iterator it = m_clients.find(id);
        if (it == m_clients.cend()) {
            p_client = std::make_shared<client>(endpoint, m_p_factory);
            m_clients.emplace(id, p_client);
        } else {
            p_client = it->second;
        }
        return p_client;
    }

    virtual void deinit() override final {}

    virtual bool init(raft::le::server_id_t) override final { return true; }

    virtual bool load() override final { return true; }

    virtual raft::le::term_t load_term() override final { return m_term; }

    virtual void set_term(raft::le::term_t term) override final { m_term = term; }

    virtual void set_voted_for(raft::le::server_id_t id) { m_voted_for = id; }

    virtual raft::le::server_id_t voted_for() const  { return m_voted_for; }

    void update_counter(const uint64_t counter)
    {
        for (std::map<raft::le::server_id_t, client::ptr>::value_type& v : m_clients) {
            v.second->send_counter(counter);
        }
    }

private:
    raft::le::cluster_config m_cluster_cfg;
    raft::le::config m_cfg;

    raft::le::term_t m_term;
    raft::le::server_id_t m_voted_for;

    mutable std::map<raft::le::server_id_t, client::ptr> m_clients;

    raft::le::ilogger_factory::ptr m_p_factory;
    raft::le::logger m_logger;
};

} // namespace details
} // namespace counter
} // namespace examples
} // namespace wstux

#endif /* _EXAMPLES_RAFT_COUNTER_IO_H_ */
