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

#ifndef _TESTS_RAFT_LEADER_ELECTION_EMPTY_IO_H_
#define _TESTS_RAFT_LEADER_ELECTION_EMPTY_IO_H_

#include <algorithm>
#include <atomic>
#include <unordered_map>

#include "raft_le/io.h"
#include "raft_le/details/connection/messages.h"
#include "raft_le/details/connection/serialization.h"

namespace wstux {
namespace raft {
namespace le {
namespace tests {

class empty_client final
{
public:
    using ptr = std::shared_ptr<empty_client>;

public:
    empty_client() : buffer(buf.begin(), buf.end()) {}
    void send(const buffer_type& b) { std::copy(b.begin(), b.end(), buf.begin()); }

    static empty_client::ptr make() { return std::make_shared<empty_client>(); }

public:
    details::buffer_data_type buf;
    buffer_type buffer;
};

class empty_io final : public io
{
public:
    using ptr = std::shared_ptr<empty_io>;

public:
    empty_io() { cfg.scheduler_threads_count = 2; }
    virtual ~empty_io() {}

    virtual cluster_config bootstrap() const override final { return cluster_cfg; }
    virtual config configuration() const override final { return cfg; };

    virtual void deinit() override final {}

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

    virtual term_t load_term() override final { return 0; }
    virtual void send(server_id_t id, const buffer_type& msg) override final { clients.at(id)->send(msg); }
    virtual void set_term(term_t) override final {}
    virtual void set_voted_for(server_id_t) override final {}
    virtual server_id_t voted_for() const override final { return gk_invalid_id; }

public:
    config cfg;
    cluster_config cluster_cfg;

    std::unordered_map<server_id_t, empty_client::ptr> clients;

    bool is_init = true;
    bool is_stop = false;
};

} // namespace tests
} // namespace le
} // namespace raft
} // namespace wstux

#endif /* _TESTS_RAFT_LEADER_ELECTION_EMPTY_IO_H_ */
