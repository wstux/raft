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
#include "raft_le/details/role/convert.h"

#include "stub/empty_io.h"

namespace {

namespace raft = ::wstux::raft::le;
namespace details = raft::details;
namespace tests = raft::tests;

class raft_peers : public ::testing::Test
{
public:
    virtual void SetUp() override
    {
        m_p_io = std::make_shared<tests::empty_io>();

        tests::empty_io* p_raw_io = m_p_io.get();
        std::function<bool()> is_stop_fn = [p_raw_io]()->bool { return p_raw_io->is_stop; };

        m_p_ctx = std::make_unique<details::context>(1, m_p_io, raft::logging_handler::ptr(), is_stop_fn);
    }

    virtual void TearDown() override {}

    details::context& init(size_t servs_count = 1, bool is_voter = true)
    {
        for (size_t i = 0; i < servs_count; ++i) {
            m_p_io->cluster_cfg.servers.emplace_back(i + 1, (i == 0) ? is_voter : true);
        }

        details::utils::init(*m_p_ctx, 1);
        details::utils::load(*m_p_ctx);
        return *m_p_ctx;
    }

protected:
    tests::empty_io::ptr m_p_io;
    details::context::ptr m_p_ctx;
};

} // <anonymous> namespace

TEST_F(raft_peers, find)
{
    details::context& ctx = init(2);

    EXPECT_FALSE(details::peers::find(ctx, 1));
    EXPECT_TRUE(details::peers::find(ctx, 2));
    EXPECT_FALSE(details::peers::find(ctx, 3));
}

TEST_F(raft_peers, quorum_for_election)
{
    details::context& ctx = init(2);

    EXPECT_TRUE(details::peers::quorum_for_election(ctx) == 1) << details::peers::quorum_for_election(ctx);
}

TEST_F(raft_peers, update)
{
    details::context& ctx = init(3);

    raft::cluster_config cluster_cfg;
    for (size_t i = 0; i < 5; ++i) {
        cluster_cfg.servers.emplace_back(i + 1, true);
    }
    EXPECT_TRUE(ctx.peers.size() == 2) << ctx.peers.size();
    m_p_io->cluster_cfg = cluster_cfg;
    m_p_io->clients.clear();
    m_p_io->init(1);
    details::peers::update(ctx, cluster_cfg);
    EXPECT_TRUE(ctx.peers.size() == 4) << ctx.peers.size();
}

TEST_F(raft_peers, voting_members_count)
{
    details::context& ctx = init(3);

    size_t voters_count = 0;
    raft::cluster_config cluster_cfg;
    for (size_t i = 0; i < 7; ++i) {
        const bool is_voter = (i + 1) % 2 == 1;
        voters_count += is_voter ? 1 : 0;
        cluster_cfg.servers.emplace_back(i + 1, is_voter);
    }
    EXPECT_TRUE(details::peers::voting_members_count(ctx) == 3) << details::peers::voting_members_count(ctx);
    m_p_io->cluster_cfg = cluster_cfg;
    m_p_io->clients.clear();
    m_p_io->init(1);
    details::peers::update(ctx, cluster_cfg);
    EXPECT_TRUE(details::peers::voting_members_count(ctx) == voters_count) << details::peers::voting_members_count(ctx);
}

TEST_F(raft_peers, check_contact_quorum)
{
    details::context& ctx = init(5);
    details::role::become_follower(ctx);
    details::role::become_candidate(ctx);
    details::role::become_leader(ctx);
    for (details::peer& p : ctx.peers) {
        p.mark_recent_recv();
    }

    EXPECT_TRUE(details::peers::check_contact_quorum(ctx));
}

TEST_F(raft_peers, failed_check_contact_quorum)
{
    details::context& ctx = init(5);
    details::role::become_follower(ctx);
    details::role::become_candidate(ctx);
    details::role::become_leader(ctx);
    details::peers::find(ctx, 2)->mark_recent_recv();

    EXPECT_FALSE(details::peers::check_contact_quorum(ctx));
}

int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
