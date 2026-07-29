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

#include "raft_le/details/context.h"
#include "raft_le/details/handlers/timeout_handler.h"
#include "raft_le/details/role/convert.h"
#include "raft_le/details/role/election.h"

#include "stub/network_stub.h"

namespace {

namespace raft = ::wstux::raft::le;

class raft_role_election : public ::testing::Test
{
public:
    virtual void SetUp() override {}
    virtual void TearDown() override {}

    static raft::details::context::ptr context(size_t cluster_size, bool is_voter = true)
    {
        namespace tests = raft::tests;

        raft::details::context::ptr p_ctx = tests::network_stub::make_context(cluster_size, is_voter);
        if (! raft::details::utils::init(*p_ctx, 1)) {
            return nullptr;
        }
        p_ctx->election_task = p_ctx->p_scheduler->make_task(std::bind(&raft::details::timeout::election_timeout_task, std::ref(*p_ctx)));
        p_ctx->heartbeat_task = p_ctx->p_scheduler->make_task(std::bind(&raft::details::timeout::heartbeat_timeout_task, std::ref(*p_ctx)));

        p_ctx->p_scheduler->cancel(p_ctx->election_task);
        p_ctx->p_scheduler->cancel(p_ctx->heartbeat_task);

        if (! raft::details::utils::load(*p_ctx)) {
            return nullptr;
        }
        return p_ctx;
    }
};

} // <anonymous> namespace

TEST_F(raft_role_election, initiate_election)
{
    raft::details::context::ptr p_ctx = raft_role_election::context(1);
    ASSERT_TRUE(p_ctx.get() != nullptr);

    raft::details::role::become_follower(*p_ctx);
    ASSERT_TRUE(p_ctx->role.is_follower());

    raft::details::role::initiate_election(*p_ctx);
    ASSERT_TRUE(p_ctx->role.is_leader());
}

TEST_F(raft_role_election, initiate_election_non_voter)
{
    raft::details::context::ptr p_ctx = raft_role_election::context(1, false);
    ASSERT_TRUE(p_ctx.get() != nullptr);

    raft::details::role::become_follower(*p_ctx);
    ASSERT_TRUE(p_ctx->role.is_follower());
    ASSERT_FALSE(p_ctx->role.is_voter);

    raft::details::role::initiate_election(*p_ctx);
    ASSERT_TRUE(p_ctx->role.is_follower()) << p_ctx->role.str();
}

TEST_F(raft_role_election, initiate_election_cluster)
{
    raft::details::context::ptr p_ctx = raft_role_election::context(3);
    ASSERT_TRUE(p_ctx.get() != nullptr);

    raft::details::role::become_follower(*p_ctx);
    ASSERT_TRUE(p_ctx->role.is_follower());

    raft::details::role::initiate_election(*p_ctx);
    ASSERT_TRUE(p_ctx->role.is_follower());
}

TEST_F(raft_role_election, election_false_results)
{
    raft::details::context::ptr p_ctx = raft_role_election::context(3);
    ASSERT_TRUE(p_ctx.get() != nullptr);

    raft::details::role::become_follower(*p_ctx);
    raft::details::role::become_candidate(*p_ctx);
    ASSERT_TRUE(p_ctx->role.is_candidate());

    p_ctx->role.candidate_state.votes_granted = 1;
    EXPECT_FALSE(raft::details::role::election_results(*p_ctx));
}

TEST_F(raft_role_election, election_true_results)
{
    raft::details::context::ptr p_ctx = raft_role_election::context(3);
    ASSERT_TRUE(p_ctx.get() != nullptr);

    raft::details::role::become_follower(*p_ctx);
    raft::details::role::become_candidate(*p_ctx);
    ASSERT_TRUE(p_ctx->role.is_candidate());

    p_ctx->role.candidate_state.votes_granted = 2;
    EXPECT_TRUE(raft::details::role::election_results(*p_ctx));

    p_ctx->role.candidate_state.votes_granted = 3;
    EXPECT_TRUE(raft::details::role::election_results(*p_ctx));
}

TEST_F(raft_role_election, election_start_prevote)
{
    raft::details::context::ptr p_ctx = raft_role_election::context(3);
    ASSERT_TRUE(p_ctx.get() != nullptr);

    raft::details::role::become_follower(*p_ctx);
    raft::details::role::become_candidate(*p_ctx);
    ASSERT_TRUE(p_ctx->role.is_candidate());

    EXPECT_TRUE(p_ctx->role.candidate_state.is_prevote);
    raft::details::role::election_start(*p_ctx);
    EXPECT_TRUE(p_ctx->role.candidate_state.is_prevote);
    EXPECT_TRUE(p_ctx->term == 0) << "Term: " << p_ctx->term;
    EXPECT_TRUE(p_ctx->role.voted_for == 0) << "Voted for: " << p_ctx->role.voted_for;
}

TEST_F(raft_role_election, election_start)
{
    raft::details::context::ptr p_ctx = raft_role_election::context(3);
    ASSERT_TRUE(p_ctx.get() != nullptr);

    raft::details::role::become_follower(*p_ctx);
    raft::details::role::become_candidate(*p_ctx);
    ASSERT_TRUE(p_ctx->role.is_candidate());

    p_ctx->role.candidate_state.is_prevote = false;
    raft::details::role::election_start(*p_ctx);
    EXPECT_FALSE(p_ctx->role.candidate_state.is_prevote);
    EXPECT_TRUE(p_ctx->term == 1) << "Term: " << p_ctx->term;
    EXPECT_TRUE(p_ctx->role.voted_for == 1) << "Voted for: " << p_ctx->role.voted_for;
}

int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
