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
class raft_join : public ::testing::Test
{
public:
    virtual void SetUp() override
    {
        m_p_network = std::make_shared<tests::network_stub>(T::type);
        tests::network_stub::enable_file_logging("raft_join", ::testing::UnitTest::GetInstance()->current_test_info());
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

TYPED_TEST_SUITE(raft_join, threaded_types);

} // <anonymous> namespace

TYPED_TEST(raft_join, join)
{
    using server_ptr = tests::network_stub::server_ptr;

    tests::network_stub::ptr p_network = this->m_p_network;
    p_network->create_cluster({{1, true}});
    EXPECT_TRUE(p_network->leaders_count() == 0);

    p_network->start();

    p_network->wait_leader();
    EXPECT_TRUE(p_network->leaders_count() == 1) << p_network->leaders_count();

    server_ptr p_leader = p_network->get_leader();
    server_ptr p_srv = p_network->create_server(2, true);
    p_srv->join(std::to_string(p_leader->id()));

    p_network->wait_for_update(3);
    for (size_t i = 1; i < 3; ++i) {
        const raft::cluster_config  cfg = p_network->get_io(i)->m_cluster_cfg;
        ASSERT_TRUE(cfg.servers.size() == 2) << "Server " << i << ": " << cfg.servers.size();
        ASSERT_TRUE(cfg.servers[0].id == 1 && cfg.servers[0].address == "1" && cfg.servers[0].is_voter)
            << "Server " << i << ": id = " << cfg.servers[0].id << "; address = " << cfg.servers[0].address;
        ASSERT_TRUE(cfg.servers[1].id == 2 && cfg.servers[1].address == "2" && cfg.servers[1].is_voter)
            << "Server " << i << ": id = " << cfg.servers[1].id << "; address = " << cfg.servers[1].address;
    }
}

TYPED_TEST(raft_join, join_train)
{
    using server_ptr = tests::network_stub::server_ptr;

    tests::network_stub::ptr p_network = this->m_p_network;
    p_network->create_cluster({{1, true}});
    EXPECT_TRUE(p_network->leaders_count() == 0);

    p_network->start();

    p_network->wait_leader();
    EXPECT_TRUE(p_network->leaders_count() == 1) << p_network->leaders_count();

    server_ptr p_leader = p_network->get_leader();
    server_ptr p_srv = p_network->create_server(2, true);
    p_srv->join(std::to_string(p_leader->id()));

    p_network->wait_for_update(3);
    for (size_t i = 1; i < 3; ++i) {
        const raft::cluster_config  cfg = p_network->get_io(i)->m_cluster_cfg;
        ASSERT_TRUE(cfg.servers.size() == 2) << "Server " << i << ": " << cfg.servers.size();
        ASSERT_TRUE(cfg.servers[0].id == 1 && cfg.servers[0].address == "1" && cfg.servers[0].is_voter)
            << "Server " << i << ": id = " << cfg.servers[0].id << "; address = " << cfg.servers[0].address;
        ASSERT_TRUE(cfg.servers[1].id == 2 && cfg.servers[1].address == "2" && cfg.servers[1].is_voter)
            << "Server " << i << ": id = " << cfg.servers[1].id << "; address = " << cfg.servers[1].address;
    }

    server_ptr p_second_srv = p_network->create_server(3, true);
    p_second_srv->join(std::to_string(p_srv->id()));

    p_network->wait_for_update(5);
    for (size_t i = 1; i < 4; ++i) {
        const raft::cluster_config  cfg = p_network->get_io(i)->m_cluster_cfg;
        ASSERT_TRUE(cfg.servers.size() == 3) << "Server " << i << ": " << cfg.servers.size();
        ASSERT_TRUE(cfg.servers[0].id == 1 && cfg.servers[0].address == "1" && cfg.servers[0].is_voter)
            << "Server " << i << ": id = " << cfg.servers[0].id << "; address = " << cfg.servers[0].address;
        ASSERT_TRUE(cfg.servers[1].id == 2 && cfg.servers[1].address == "2" && cfg.servers[1].is_voter)
            << "Server " << i << ": id = " << cfg.servers[1].id << "; address = " << cfg.servers[1].address;
        ASSERT_TRUE(cfg.servers[2].id == 3 && cfg.servers[2].address == "3" && cfg.servers[2].is_voter)
            << "Server " << i << ": id = " << cfg.servers[2].id << "; address = " << cfg.servers[2].address;
    }
}

int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
