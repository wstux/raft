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

#ifndef _TESTS_RAFT_LEADER_ELECTION_NETWORK_STUB_H_
#define _TESTS_RAFT_LEADER_ELECTION_NETWORK_STUB_H_

#include <algorithm>
#include <filesystem>
#include <map>
#include <vector>

#include "raft_le/io.h"
#include "raft_le/server.h"
#include "raft_le/details/context.h"

#include "stub/io_stub.h"

#if ! defined(TEST_DATA_DIR)
    #error "TEST_DATA_DIR must be defined"
#endif

namespace wstux {
namespace raft {
namespace le {
namespace tests {

class network_stub final : public io_stub::iclient_factory, public std::enable_shared_from_this<network_stub>
{
public:
    using ptr = std::shared_ptr<network_stub>;
    using server_ptr = server::ptr;

public:
    network_stub(const client_type type = client_type::single, const std::string& test_fixture = "", const std::string& test_name = "")
        : m_type(type)
        , m_p_cluster_cfg(std::make_shared<cluster_config>())
    {
        m_test_fixture = test_fixture;
        m_test_name = test_name;
    }

    virtual ~network_stub() {}

    virtual iclient::ptr create_client(server_id_t id, const std::string&) const override
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

    server_ptr get_server(server_id_t id) const { return m_servers.at(id); }

    std::vector<server_ptr> get_servers() const
    {
        using servers_map = std::map<server_id_t, server_ptr>;

        std::vector<server_ptr> servers;
        std::transform(m_servers.cbegin(), m_servers.cend(), std::back_inserter(servers),
                       [](const servers_map::value_type& v) -> server_ptr { return v.second; });
        return servers;
    }

    void build_cluster()
    {
        io_stub::iclient_factory::ptr p_factory = shared_from_this();
        for (const server_config& cfg : m_p_cluster_cfg->servers) {
            io_stub::ptr p_io = std::make_shared<io_stub>(m_p_cluster_cfg, p_factory);

            server_ptr p_srv = create_server_impl(cfg.id, p_io);
        }
    }

    void create_cluster(const std::vector<std::pair<server_id_t, bool>>& servers)
    {
        for (const std::pair<server_id_t, bool>& srv_param : servers) {
            register_server(srv_param.first, srv_param.second);
        }
        build_cluster();
    }

    server_ptr create_server(server_id_t id, bool is_voter, bool is_separate = true, bool is_start = true)
    {
        std::shared_ptr<cluster_config> p_cluster_cfg;
        if (! is_separate) {
            p_cluster_cfg = m_p_cluster_cfg;
            register_server(id, is_voter);
        } else {
            p_cluster_cfg = std::make_shared<cluster_config>();
            p_cluster_cfg->servers.emplace_back(id, std::to_string(id), is_voter);
        }
        io_stub::iclient_factory::ptr p_factory = shared_from_this();
        io_stub::ptr p_io = std::make_shared<io_stub>(p_cluster_cfg, p_factory);
        server_ptr p_srv = create_server_impl(id, p_io);

        if (is_start) {
            p_srv->init();
            p_srv->start();
        }
        return p_srv;
    }

    bool has_leader() const { return (leaders_count() != 0); }

    size_t leaders_count() const
    {
        using servers_map = std::map<server_id_t, server_ptr>;

        return std::count_if(m_servers.begin(), m_servers.end(),
                             [](const servers_map::value_type& v) -> bool { return v.second->is_leader(); });
    }

    void register_server(server_id_t id, bool is_voter = true)
    {
        m_p_cluster_cfg->servers.emplace_back(id, std::to_string(id), is_voter);
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

    void start(bool is_init = true)
    {
        for (const std::map<server_id_t, server_ptr>::value_type& s : m_servers) {
            if (is_init) {
                s.second->init();
            }
            s.second->start();
        }
    }

    void stop()
    {
        using namespace std::chrono_literals;

        m_is_stop = true;
        for (const std::map<server_id_t, server_ptr>::value_type& s : m_servers) {
            s.second->stop();
            s.second->deinit();
        }

        if (m_type != client_type::single) {
            std::this_thread::sleep_for(300ms);
        }
    }

    void wait_leader(const size_t limit = 3) const
    {
        using namespace std::chrono_literals;
        for (size_t i = 0; (i < limit * 3) && (! has_leader()); ++i) {
            std::this_thread::sleep_for(500ms);
        }
    }

    static details::context::ptr make_context(const size_t servs_count, io_stub::iclient_factory::ptr p_factory,
                                              io_stub::results::ptr p_res, bool is_voter = true)
    {
        std::shared_ptr<cluster_config> p_cluster_cfg = std::make_shared<cluster_config>();
        for (size_t i = 0; i < servs_count; ++i) {
            p_cluster_cfg->servers.emplace_back(i + 1, std::to_string(i), (i == 0) ? is_voter : true);
        }

        std::function<bool()> is_stop_fn = []()->bool { return false; };
        tests::io_stub::ptr p_io = std::make_shared<io_stub>(p_cluster_cfg, p_factory, p_res);

        details::context::ptr p_ctx = std::make_shared<details::context>(1, p_io, std::make_shared<logger_factory>(), is_stop_fn);
        return p_ctx;
    }

    template<typename TTestInfo>
    static network_stub::ptr make_network(const client_type type = client_type::single, const std::string& test_fixture = "",
                                          const TTestInfo* p_test_info = nullptr)
    {
        const std::string test_name = (p_test_info) ? (p_test_info->name()) : "";
        return std::make_shared<tests::network_stub>(type, test_fixture, test_name);
    }

private:
    server_ptr create_server_impl(server_id_t id, io_stub::ptr p_io)
    {
        std::function<bool()> is_stop_fn = []()->bool { return false; };

        server_ptr p_srv = std::make_shared<server>(id, p_io, std::make_shared<logger_factory>(log_file(id)), is_stop_fn);

        m_io_map.emplace(id, p_io);
        m_servers.emplace(id, p_srv);

        return p_srv;
    }

    static std::string log_dir()
    {
        namespace fs = std::filesystem;

        if (! logger_factory::is_enable_logging) {
            return std::string();
        }

        fs::path td_dir = TEST_DATA_DIR;
        if (! fs::exists(td_dir.parent_path())) {
            return std::string();
        }

        fs::path logdir = td_dir / "tests_log";
        if (! m_test_fixture.empty()) {
            logdir = logdir / m_test_fixture;
        }
        if (! m_test_name.empty()) {
            logdir = logdir / m_test_name;
        }
        if (! fs::exists(logdir)) {
            if (! fs::create_directories(logdir)) {
                return std::string();
            }
        }
        return logdir.string();
    }

    static std::string log_file(server_id_t id)
    {
        namespace fs = std::filesystem;

        std::string logdir_str = log_dir();
        if (logdir_str.empty()) {
            return std::string();
        }
        fs::path log_file = fs::path(logdir_str) / ("server_" + std::to_string(id));
        return log_file.string();
    }

private:
    const client_type m_type;
    std::atomic_bool m_is_stop{false};
    std::shared_ptr<cluster_config> m_p_cluster_cfg;

    std::map<server_id_t, io_stub::ptr> m_io_map;
    std::map<server_id_t, server_ptr> m_servers;

    static std::string m_test_fixture;
    static std::string m_test_name;
};

std::string network_stub::m_test_fixture = "";
std::string network_stub::m_test_name = "";

} // namespace tests
} // namespace le
} // namespace raft
} // namespace wstux

#endif /* _TESTS_RAFT_LEADER_ELECTION_NETWORK_STUB_H_ */
