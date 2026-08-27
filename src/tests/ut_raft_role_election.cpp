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

#include <gtest/gtest.h>

#include "raft/details/context.h"
#include "raft/details/handlers/timeout_handler.h"
#include "raft/details/role/convert.h"
#include "raft/details/role/election.h"

#include "stub/empty_io.h"

namespace {

namespace raft = ::wstux::raft;
namespace details = raft::details;
namespace tests = raft::tests;

class raft_role_election : public ::testing::Test
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

    details::context& init(size_t servs_count, bool is_voter = true)
    {
        for (size_t i = 0; i < servs_count; ++i) {
            m_p_io->cluster_cfg.servers.emplace_back(i + 1, (i == 0) ? is_voter : true);
        }

        details::utils::init(*m_p_ctx);
        m_p_ctx->election_task = m_p_ctx->schd.make_task(std::bind(&details::timeout::election_timeout_task, std::ref(*m_p_ctx)));
        m_p_ctx->heartbeat_task = m_p_ctx->schd.make_task(std::bind(&details::timeout::heartbeat_timeout_task, std::ref(*m_p_ctx)));

        m_p_ctx->schd.cancel(m_p_ctx->election_task);
        m_p_ctx->schd.cancel(m_p_ctx->heartbeat_task);

        details::utils::load(*m_p_ctx);
        return *m_p_ctx;
    }

protected:
    tests::empty_io::ptr m_p_io;
    details::context::ptr m_p_ctx;
};

} // <anonymous> namespace

TEST_F(raft_role_election, initiate_election)
{
    details::context& ctx = init(1);

    details::role::become_follower(ctx);
    ASSERT_TRUE(ctx.role.is_follower());

    details::role::initiate_election(ctx);
    ASSERT_TRUE(ctx.role.is_leader());
}

TEST_F(raft_role_election, initiate_election_non_voter)
{
    details::context& ctx = init(1, false);

    details::role::become_follower(ctx);
    ASSERT_TRUE(ctx.role.is_follower());
    ASSERT_FALSE(ctx.role.is_voter);

    details::role::initiate_election(ctx);
    ASSERT_TRUE(ctx.role.is_follower()) << ctx.role.str();
}

TEST_F(raft_role_election, initiate_election_cluster)
{
    details::context& ctx = init(3);

    details::role::become_follower(ctx);
    ASSERT_TRUE(ctx.role.is_follower());

    details::role::initiate_election(ctx);
    ASSERT_TRUE(ctx.role.is_follower());
}

TEST_F(raft_role_election, election_false_results)
{
    details::context& ctx = init(3);

    details::role::become_follower(ctx);
    details::role::become_candidate(ctx);
    ASSERT_TRUE(ctx.role.is_candidate());

    ctx.role.candidate_state.votes_granted = 1;
    EXPECT_FALSE(details::role::election_results(ctx));
}

TEST_F(raft_role_election, election_true_results)
{
    details::context& ctx = init(3);

    details::role::become_follower(ctx);
    details::role::become_candidate(ctx);
    ASSERT_TRUE(ctx.role.is_candidate());

    ctx.role.candidate_state.votes_granted = 2;
    EXPECT_TRUE(details::role::election_results(ctx));

    ctx.role.candidate_state.votes_granted = 3;
    EXPECT_TRUE(details::role::election_results(ctx));
}

TEST_F(raft_role_election, election_start_prevote)
{
    details::context& ctx = init(3);

    details::role::become_follower(ctx);
    details::role::become_candidate(ctx);
    ASSERT_TRUE(ctx.role.is_candidate());

    EXPECT_TRUE(ctx.role.candidate_state.is_prevote);
    details::role::election_start(ctx);
    EXPECT_TRUE(ctx.role.candidate_state.is_prevote);
    EXPECT_TRUE(ctx.term == 0) << "Term: " << ctx.term;
    EXPECT_TRUE(ctx.role.voted_for == 0) << "Voted for: " << ctx.role.voted_for;
}

TEST_F(raft_role_election, election_start)
{
    details::context& ctx = init(3);

    details::role::become_follower(ctx);
    details::role::become_candidate(ctx);
    ASSERT_TRUE(ctx.role.is_candidate());

    ctx.role.candidate_state.is_prevote = false;
    details::role::election_start(ctx);
    EXPECT_FALSE(ctx.role.candidate_state.is_prevote);
    EXPECT_TRUE(ctx.term == 1) << "Term: " << ctx.term;
    EXPECT_TRUE(ctx.role.voted_for == 1) << "Voted for: " << ctx.role.voted_for;
}

int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
