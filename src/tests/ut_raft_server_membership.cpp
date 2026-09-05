/*
 * The MIT License
 *
 * Copyright 2024 Chistyakov Alexander.
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

#include <algorithm>
#include <map>
#include <thread>

#include <gtest/gtest.h>

#include "raft/server.h"

#include "stub/network_stub.h"

namespace {

namespace raft = wstux::raft;
namespace tests = raft::tests;

template <typename T>
class raft_membership : public ::testing::Test
{
public:
    virtual void SetUp() override
    {
        m_p_network = std::make_shared<tests::network_stub>(T::type);
        tests::network_stub::enable_file_logging("raft_membership", ::testing::UnitTest::GetInstance()->current_test_info());
    }

    virtual void TearDown() override { m_p_network->stop(); }

protected:
    tests::network_stub::ptr m_p_network;
};

struct single_thread
{
    static constexpr tests::client_type type = tests::client_type::single;
};

struct multi_thread
{
    static constexpr tests::client_type type = tests::client_type::threaded;
};

struct random_multi_thread
{
    static constexpr tests::client_type type = tests::client_type::random_threaded;
};

typedef ::testing::Types</*single_thread,*/ multi_thread, random_multi_thread> threaded_types;

TYPED_TEST_SUITE(raft_membership, threaded_types);

} // <anonymous> namespace

TYPED_TEST(raft_membership, add_server)
{
    using namespace std::chrono_literals;
    using server_ptr = tests::network_stub::server_ptr;
    using io_ptr = tests::io_stub::ptr;

    tests::network_stub::ptr p_network = this->m_p_network;
    p_network->create_cluster({{1, true}, {2, true}, {3, true}});
    EXPECT_TRUE(p_network->leaders_count() == 0);

    p_network->start();

    p_network->wait_leader();
    EXPECT_TRUE(p_network->leaders_count() == 1) << p_network->leaders_count();

    server_ptr p_leader = p_network->get_leader();
    io_ptr p_io = p_network->get_io(p_leader->id());
    server_ptr p_srv = p_network->create_server(4, true);

    raft::cluster_config cfg = p_io->m_cluster_cfg;
    ASSERT_TRUE(cfg.servers.size() == 3) << cfg.servers.size();

    p_leader->add(p_srv->id(), std::to_string(p_srv->id()), true);

    std::this_thread::sleep_for(500ms);
    EXPECT_FALSE(p_srv->is_leader());

    for (size_t i = 1; i < 5; ++i) {
        cfg = p_network->get_io(i)->m_cluster_cfg;
        ASSERT_TRUE(cfg.servers.size() == 4) << cfg.servers.size();
    }
}

TYPED_TEST(raft_membership, add_leader_server)
{
    using namespace std::chrono_literals;
    using server_ptr = tests::network_stub::server_ptr;
    using io_ptr = tests::io_stub::ptr;

    tests::network_stub::ptr p_network = this->m_p_network;
    p_network->create_cluster({{1, true}, {2, true}, {3, true}});
    EXPECT_TRUE(p_network->leaders_count() == 0);

    p_network->start();

    p_network->wait_leader();
    EXPECT_TRUE(p_network->leaders_count() == 1) << p_network->leaders_count();

    server_ptr p_leader = p_network->get_leader();
    io_ptr p_io = p_network->get_io(p_leader->id());
    server_ptr p_srv = p_network->create_server(4, true);

    std::this_thread::sleep_for(1000ms);
    EXPECT_TRUE(p_srv->is_leader());

    raft::cluster_config cfg = p_io->m_cluster_cfg;
    ASSERT_TRUE(cfg.servers.size() == 3) << cfg.servers.size();

    p_leader->add(p_srv->id(), std::to_string(p_srv->id()), true);

    std::this_thread::sleep_for(500ms);
    EXPECT_FALSE(p_srv->is_leader());

    for (size_t i = 1; i < 5; ++i) {
        cfg = p_network->get_io(i)->m_cluster_cfg;
        ASSERT_TRUE(cfg.servers.size() == 4) << cfg.servers.size();
    }
}

TYPED_TEST(raft_membership, remove_server)
{
    using namespace std::chrono_literals;
    using server_ptr = tests::network_stub::server_ptr;

    tests::network_stub::ptr p_network = this->m_p_network;
    p_network->create_cluster({{1, true}, {2, true}, {3, true}});
    EXPECT_TRUE(p_network->leaders_count() == 0);

    p_network->start();

    p_network->wait_leader();
    EXPECT_TRUE(p_network->leaders_count() == 1) << p_network->leaders_count();

    server_ptr p_leader = p_network->get_leader();
    server_ptr p_srv;
    for (size_t i = 1; i < 4; ++i) {
        if (! p_network->get_server(i)->is_leader()) {
            p_srv = p_network->get_server(i);
            break;
        }
    }
    ASSERT_TRUE(p_srv.get() != nullptr);
    ASSERT_TRUE(p_leader->is_leader());
    ASSERT_FALSE(p_srv->is_leader());
    ASSERT_TRUE(p_leader->id() != p_srv->id());

    raft::cluster_config cfg = p_network->get_io(p_leader->id())->m_cluster_cfg;
    ASSERT_TRUE(cfg.servers.size() == 3) << cfg.servers.size();

    p_leader->remove(p_srv->id());

    std::this_thread::sleep_for(500ms);
    for (size_t i = 1; i < 4; ++i) {
        if (i == p_srv->id()) {
            continue;
        }
        cfg = p_network->get_io(i)->m_cluster_cfg;
        EXPECT_TRUE(cfg.servers.size() == 2) << cfg.servers.size();
        for (const raft::server_config& cfg : cfg.servers) {
            EXPECT_TRUE(cfg.id != p_srv->id());
        }
    }
}

int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
