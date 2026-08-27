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
class raft_election : public ::testing::Test
{
public:
    virtual void SetUp() override
    {
        m_p_network = std::make_shared<tests::network_stub>(T::type);
        tests::network_stub::enable_file_logging("raft_election", ::testing::UnitTest::GetInstance()->current_test_info());
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

TYPED_TEST_SUITE(raft_election, threaded_types);

} // <anonymous> namespace

TYPED_TEST(raft_election, one_server)
{
    using namespace std::chrono_literals;

    tests::network_stub::ptr p_network = this->m_p_network;
    p_network->create_cluster({{1, true}});
    std::shared_ptr<raft::server> p_srv = p_network->get_server(1);
    EXPECT_FALSE(p_srv->is_leader());

    p_srv->init();
    p_srv->start();

    p_network->wait_leader();
    EXPECT_TRUE(p_srv->is_leader());
}

TYPED_TEST(raft_election, election_leader)
{
    using namespace std::chrono_literals;

    tests::network_stub::ptr p_network = this->m_p_network;
    p_network->create_cluster({{1, true}, {2, true}, {3, true}});
    EXPECT_TRUE(p_network->leaders_count() == 0);

    p_network->start();

    p_network->wait_leader();
    EXPECT_TRUE(p_network->leaders_count() == 1) << p_network->leaders_count();
}

TYPED_TEST(raft_election, not_voted_servers)
{
    using namespace std::chrono_literals;

    tests::network_stub::ptr p_network = this->m_p_network;
    p_network->create_cluster({{1, true}, {2, false}, {3, false}});
    EXPECT_TRUE(p_network->leaders_count() == 0);

    p_network->start();

    p_network->wait_leader();
    EXPECT_TRUE(p_network->leaders_count() == 1) << p_network->leaders_count();

    std::shared_ptr<raft::server> p_leader_srv = p_network->get_server(1);
    EXPECT_TRUE(p_leader_srv->is_leader());
}

TYPED_TEST(raft_election, election_leader_long_work)
{
    using namespace std::chrono_literals;

    tests::network_stub::ptr p_network = this->m_p_network;
    p_network->create_cluster({{1, true}, {2, true}, {3, true}});
    EXPECT_TRUE(p_network->leaders_count() == 0);

    p_network->start();
    for (size_t i = 0; i < 3; ++i) {
        std::this_thread::sleep_for(1500ms);
        EXPECT_TRUE(p_network->leaders_count() == 1) << p_network->leaders_count();
    }
}

TYPED_TEST(raft_election, reelection_leader)
{
    using namespace std::chrono_literals;

    tests::network_stub::ptr p_network = this->m_p_network;
    p_network->create_cluster({{1, true}, {2, true}, {3, true}});
    EXPECT_TRUE(p_network->leaders_count() == 0);

    p_network->start();

    p_network->wait_leader();
    EXPECT_TRUE(p_network->leaders_count() == 1) << p_network->leaders_count();

    std::shared_ptr<raft::server> p_leader = p_network->get_leader();
    ASSERT_TRUE(p_leader);

    p_leader->stop();
    p_network->remove_leader();

    p_network->wait_leader();
    EXPECT_TRUE(p_network->leaders_count() == 1) << p_network->leaders_count();
}

int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
