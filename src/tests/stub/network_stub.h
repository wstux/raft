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

#ifndef _TESTS_RAFT_NETWORK_STUB_H_
#define _TESTS_RAFT_NETWORK_STUB_H_

#include <algorithm>
#include <filesystem>
#include <map>
#include <vector>

#include "raft/io.h"
#include "raft/server.h"
#include "raft/details/context.h"

#include "stub/fsm_stub.h"
#include "stub/io_stub.h"
#include "stub/logging_handler_stub.h"

#if ! defined(TEST_DATA_DIR)
    #error "TEST_DATA_DIR must be defined"
#endif

namespace wstux {
namespace raft {
namespace tests {

class network_stub final : public io_stub::iclient_factory, public std::enable_shared_from_this<network_stub>
{
public:
    using ptr = std::shared_ptr<network_stub>;
    using server_ptr = server::ptr;

public:
    explicit network_stub(const client_type type = client_type::single)
        : m_type(type)
    {}

    virtual ~network_stub() { stop(); }

    virtual iclient::ptr create_client(server_id_t id) const override
    {
        server_ptr p_srv = get_server(id);
        iclient::ptr p_client;
        if (m_type == client_type::single) {
            p_client = std::make_shared<client_stub>(p_srv);
        } else {
            p_client = std::make_shared<threaded_client_stub>(p_srv, m_type, [this]() -> bool { return m_is_stop; });
        }
        return p_client;
    }

    server_ptr create_server(server_id_t id, bool is_voter, bool is_start = true)
    {
        cluster_config cluster_cfg;
        cluster_cfg.servers.emplace_back(id, std::to_string(id), is_voter);

        io_stub::iclient_factory::ptr p_factory = this->shared_from_this();
        io_stub::ptr p_io = std::make_shared<io_stub>(cluster_cfg, p_factory);
        server_ptr p_srv = create_server_impl(id, p_io);

        p_srv->init();
        if (is_start) {
            p_srv->start();
        }
        return p_srv;
    }

    server_ptr get_leader() const
    {
        using servers_map = std::map<server_id_t, server_ptr>;

        std::map<server_id_t, server_ptr>::const_iterator it =
            std::find_if(m_servers.cbegin(), m_servers.cend(),
                         [](const servers_map::value_type& v) -> bool { return v.second->is_leader(); });
        if (it != m_servers.cend()) {
            return it->second;
        }
        return nullptr;
    }

    io_stub::ptr get_io(server_id_t id) const { return m_io_map.at(id); }

    server_ptr get_server(server_id_t id) const { return m_servers.at(id); }

    std::vector<server_ptr> get_servers() const
    {
        using servers_map = std::map<server_id_t, server_ptr>;

        std::vector<server_ptr> servers;
        std::transform(m_servers.cbegin(), m_servers.cend(), std::back_inserter(servers),
                       [](const servers_map::value_type& v) -> server_ptr { return v.second; });
        return servers;
    }

    void create_cluster(const std::vector<std::pair<server_id_t, bool>>& servers, bool is_init = true)
    {
        for (const std::pair<server_id_t, bool>& srv_param : servers) {
            const server_id_t id = srv_param.first;
            const bool is_voter = srv_param.second;
            m_cluster_cfg.servers.emplace_back(id, std::to_string(id), is_voter);
        }
        for (const server_config& cfg : m_cluster_cfg.servers) {
            io_stub::ptr p_io = std::make_shared<io_stub>(m_cluster_cfg, this->shared_from_this());
            create_server_impl(cfg.id, p_io);
        }
        if (is_init) {
            init();
        }
    }

    void init()
    {
        for (const std::map<server_id_t, server_ptr>::value_type& s : m_servers) {
            s.second->init();
        }
    }

    bool has_leader() const { return (leaders_count() != 0); }

    size_t leaders_count() const
    {
        using servers_map = std::map<server_id_t, server_ptr>;

        return std::count_if(m_servers.begin(), m_servers.end(),
                             [](const servers_map::value_type& v) -> bool { return v.second->is_leader(); });
    }

    void remove_leader()
    {
        using servers_map = std::map<server_id_t, server_ptr>;

        std::map<server_id_t, server_ptr>::iterator it =
            std::find_if(m_servers.begin(), m_servers.end(),
                         [](const servers_map::value_type& v) -> bool { return v.second->is_leader(); });
        if (it != m_servers.end()) {
            m_servers.erase(it);
        }
    }

    void start()
    {
        for (const std::map<server_id_t, server_ptr>::value_type& s : m_servers) {
            s.second->start();
        }
    }

    void stop()
    {
        using namespace std::chrono_literals;

        if (m_is_stop) {
            return;
        }
        m_is_stop = true;
        for (const std::map<server_id_t, server_ptr>::value_type& s : m_servers) {
            s.second->stop();
            s.second->deinit();
        }

        m_servers.clear();
        m_io_map.clear();
    }

    void wait_changed_cluster_cfg(const size_t limit_ms = 1500) const
    {
        using namespace std::chrono_literals;
        bool is_changed = false;
        for (size_t i = 0; (i < limit_ms) && ! is_changed; i += 10) {
            is_changed = std::all_of(m_io_map.cbegin(), m_io_map.cend(),
                [] (const std::map<server_id_t, io_stub::ptr>::value_type& io) -> bool {return io.second->m_is_changed_cluster_cfg; }
            );
            if (! is_changed) {
                std::this_thread::sleep_for(10ms);
            }
        }
    }

    void wait_changed_cluster_cfg_except(server_id_t except_id, const size_t limit_ms = 1500) const
    {
        using namespace std::chrono_literals;
        bool is_changed = false;
        for (size_t i = 0; (i < limit_ms) && ! is_changed; i += 10) {
            is_changed = std::all_of(m_io_map.cbegin(), m_io_map.cend(),
                [except_id] (const std::map<server_id_t, io_stub::ptr>::value_type& io) -> bool {
                    if (except_id != io.first) {
                        return io.second->m_is_changed_cluster_cfg;
                    }
                    return true;
                }
            );
            if (! is_changed) {
                std::this_thread::sleep_for(10ms);
            }
        }
    }

    void wait_leader(const size_t limit_ms = 1500) const
    {
        using namespace std::chrono_literals;
        for (size_t i = 0; (i < limit_ms) && (! has_leader()); i += 10) {
            std::this_thread::sleep_for(10ms);
        }
    }

    template<typename TTestInfo>
    static void enable_file_logging(const std::string& fixture, const TTestInfo* p_info)
    {
        const std::string name = (p_info) ? (p_info->name()) : "";
        m_test_fixture = fixture;
        m_test_name = name;
    }

private:
    server_ptr create_server_impl(server_id_t id, io_stub::ptr p_io)
    {
        std::function<bool()> is_stop_fn = []()->bool { return false; };

        fsm::ptr p_fsm = std::make_shared<fsm_stub>();
        server_ptr p_srv = std::make_shared<server>(id, p_io, p_fsm, std::make_unique<tests::logging_handler_file>(log_file(id)), is_stop_fn);

        m_io_map.emplace(id, p_io);
        m_servers.emplace(id, p_srv);

        return p_srv;
    }

    static std::string log_file(server_id_t id)
    {
        namespace fs = std::filesystem;

        std::string logdir_str = tests::logging_handler_file::log_dir(TEST_DATA_DIR, m_test_fixture, m_test_name);
        if (logdir_str.empty()) {
            return std::string();
        }
        return fs::path(logdir_str) / ("server_" + std::to_string(id));
    }

private:
    const client_type m_type;
    std::atomic_bool m_is_stop{false};
    cluster_config m_cluster_cfg;

    std::map<server_id_t, io_stub::ptr> m_io_map;
    std::map<server_id_t, server_ptr> m_servers;

    static std::string m_test_fixture;
    static std::string m_test_name;
};

std::string network_stub::m_test_fixture = "";
std::string network_stub::m_test_name = "";

} // namespace tests
} // namespace raft
} // namespace wstux

#endif /* _TESTS_RAFT_NETWORK_STUB_H_ */
