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

#include "stub/empty_io.h"

namespace {

namespace raft = ::wstux::raft::le;

class raft_role_convert : public ::testing::Test
{
public:
    virtual void SetUp() override {}
    virtual void TearDown() override {}

    static raft::details::context::ptr context(size_t cluster_size, bool is_voter = true)
    {
        namespace tests = raft::tests;

        tests::return_type rt = tests::make_context(cluster_size, is_voter);
        raft::details::context::ptr p_ctx = std::move(rt.first);
        raft::details::utils::init(*p_ctx, 1);
        p_ctx->election_task = p_ctx->p_scheduler->make_task(std::bind(&raft::details::timeout::election_timeout_task, std::ref(*p_ctx)));
        p_ctx->heartbeat_task = p_ctx->p_scheduler->make_task(std::bind(&raft::details::timeout::heartbeat_timeout_task, std::ref(*p_ctx)));

        p_ctx->p_scheduler->cancel(p_ctx->election_task);
        p_ctx->p_scheduler->cancel(p_ctx->heartbeat_task);

        raft::details::utils::load(*p_ctx);
        return p_ctx;
    }
};

} // <anonymous> namespace

TEST_F(raft_role_convert, become_follower)
{
    raft::details::context::ptr p_ctx = raft_role_convert::context(3);
    ASSERT_TRUE(p_ctx.get() != nullptr);

    raft::details::role::become_follower(*p_ctx);
    EXPECT_TRUE(p_ctx->role.is_follower());
}

TEST_F(raft_role_convert, become_candidate)
{
    raft::details::context::ptr p_ctx = raft_role_convert::context(3);
    ASSERT_TRUE(p_ctx.get() != nullptr);

    raft::details::role::become_follower(*p_ctx);
    raft::details::role::become_candidate(*p_ctx);
    EXPECT_TRUE(p_ctx->role.is_candidate());
}

TEST_F(raft_role_convert, become_candidate_single)
{
    raft::details::context::ptr p_ctx = raft_role_convert::context(1);
    ASSERT_TRUE(p_ctx.get() != nullptr);

    raft::details::role::become_follower(*p_ctx);
    raft::details::role::become_candidate(*p_ctx);
    EXPECT_TRUE(p_ctx->role.is_leader());
}

TEST_F(raft_role_convert, become_leader)
{
    raft::details::context::ptr p_ctx = raft_role_convert::context(3);
    ASSERT_TRUE(p_ctx.get() != nullptr);

    raft::details::role::become_follower(*p_ctx);
    raft::details::role::become_candidate(*p_ctx);
    raft::details::role::become_leader(*p_ctx);
    EXPECT_TRUE(p_ctx->role.is_leader());
}

TEST_F(raft_role_convert, become_leader_non_voters)
{
    raft::details::context::ptr p_ctx = raft_role_convert::context(1);
    ASSERT_TRUE(p_ctx.get() != nullptr);

    raft::details::role::become_follower(*p_ctx);

    raft::details::role::become_candidate(*p_ctx);
    EXPECT_TRUE(p_ctx->role.is_leader());
}

TEST_F(raft_role_convert, update_leader)
{
    raft::details::context::ptr p_ctx = raft_role_convert::context(3);
    ASSERT_TRUE(p_ctx.get() != nullptr);

    raft::details::role::become_follower(*p_ctx);
    EXPECT_TRUE(p_ctx->role.is_follower());
    EXPECT_TRUE(p_ctx->role.follower_state.leader_id == raft::gk_invalid_id);

    raft::details::role::update_leader(*p_ctx, 3);
    EXPECT_TRUE(p_ctx->role.is_follower());
    EXPECT_TRUE(p_ctx->role.follower_state.leader_id == 3);
}

TEST_F(raft_role_convert, update_term)
{
    raft::details::context::ptr p_ctx = raft_role_convert::context(3);
    ASSERT_TRUE(p_ctx.get() != nullptr);

    raft::details::role::become_follower(*p_ctx);
    EXPECT_TRUE(p_ctx->role.is_follower());

    p_ctx->term = 1;
    raft::details::role::update_term(*p_ctx, 3);
    EXPECT_TRUE(p_ctx->role.is_follower());
    EXPECT_TRUE(p_ctx->term == 3);
}

TEST_F(raft_role_convert, update_term_local_term_higher)
{
    raft::details::context::ptr p_ctx = raft_role_convert::context(3);
    ASSERT_TRUE(p_ctx.get() != nullptr);

    raft::details::role::become_follower(*p_ctx);
    EXPECT_TRUE(p_ctx->role.is_follower());

    p_ctx->term = 3;
    raft::details::role::update_term(*p_ctx, 1);
    EXPECT_TRUE(p_ctx->role.is_follower());
    EXPECT_TRUE(p_ctx->term == 3);
}

TEST_F(raft_role_convert, update_term_become_leader)
{
    raft::details::context::ptr p_ctx = raft_role_convert::context(3);
    ASSERT_TRUE(p_ctx.get() != nullptr);

    raft::details::role::become_follower(*p_ctx);
    raft::details::role::become_candidate(*p_ctx);
    raft::details::role::become_leader(*p_ctx);
    EXPECT_TRUE(p_ctx->role.is_leader());

    p_ctx->term = 1;
    raft::details::role::update_term(*p_ctx, 3);
    EXPECT_TRUE(p_ctx->role.is_follower());
    EXPECT_TRUE(p_ctx->term == 3);
}

int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
