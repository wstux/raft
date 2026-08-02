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

#include <chrono>
#include <thread>

#include <gtest/gtest.h>

#include "raft_le/details/context.h"

#include "stub/empty_io.h"

namespace {

namespace raft = ::wstux::raft::le;
namespace details = raft::details;
namespace tests = raft::tests;

class raft_peers : public ::testing::Test
{
public:
    virtual void SetUp() override {}
    virtual void TearDown() override {}

    static details::context::ptr context(size_t cluster_size = 0, bool is_voter = true)
    {
        tests::return_type rt = tests::make_context(cluster_size, is_voter);
        details::context::ptr p_ctx = std::move(rt.first);
        details::utils::init(*p_ctx, 1);
        details::utils::load(*p_ctx);
        return p_ctx;
    }
};

} // <anonymous> namespace

TEST_F(raft_peers, probe_expired)
{
    using namespace std::chrono_literals;

    details::peer peer(raft::server_config(1, "1", true), std::make_shared<tests::empty_io>(), 5);
    EXPECT_FALSE(peer.is_probe_expired());

    peer.update_last_response();
    std::this_thread::sleep_for(10ms);
    EXPECT_TRUE(peer.is_probe_expired());
}

TEST_F(raft_peers, emplace)
{
    details::context::ptr p_ctx = raft_peers::context();

    raft::server_config self_cfg(1, "1", true);
    EXPECT_FALSE(details::peers::emplace(*p_ctx, self_cfg));

    raft::server_config new_cfg(2, "2", true);
    EXPECT_TRUE(details::peers::emplace(*p_ctx, new_cfg));
}

TEST_F(raft_peers, find)
{
    details::context::ptr p_ctx = raft_peers::context(2);

    EXPECT_FALSE(details::peers::find(*p_ctx, 1));
    EXPECT_TRUE(details::peers::find(*p_ctx, 2));
    EXPECT_FALSE(details::peers::find(*p_ctx, 3));
}

TEST_F(raft_peers, quorum_for_election)
{
    details::context::ptr p_ctx = raft_peers::context(2);

    EXPECT_TRUE(details::peers::quorum_for_election(*p_ctx) == 1) << details::peers::quorum_for_election(*p_ctx);
}

TEST_F(raft_peers, update)
{
    details::context::ptr p_ctx = raft_peers::context(3);

    raft::cluster_config cluster_cfg;
    for (size_t i = 0; i < 5; ++i) {
        cluster_cfg.servers.emplace_back(i + 1, std::to_string(i), true);
    }
    EXPECT_TRUE(p_ctx->peers.size() == 2) << p_ctx->peers.size();
    details::peers::update(*p_ctx, cluster_cfg);
    EXPECT_TRUE(p_ctx->peers.size() == 4) << p_ctx->peers.size();
}

TEST_F(raft_peers, voting_members_count)
{
    details::context::ptr p_ctx = raft_peers::context(3);

    raft::cluster_config cluster_cfg;
    for (size_t i = 0; i < 7; ++i) {
        cluster_cfg.servers.emplace_back(i + 1, std::to_string(i), (i + 1) % 2 == 1);
    }
    EXPECT_TRUE(details::peers::voting_members_count(*p_ctx) == 3) << details::peers::voting_members_count(*p_ctx);
    details::peers::update(*p_ctx, cluster_cfg);
    EXPECT_TRUE(details::peers::voting_members_count(*p_ctx) == 5) << details::peers::voting_members_count(*p_ctx);
}

int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
