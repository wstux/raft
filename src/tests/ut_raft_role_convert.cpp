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
namespace details = raft::details;
namespace tests = raft::tests;

class raft_role_convert : public ::testing::Test
{
public:
    virtual void SetUp() override
    {
        m_p_io = std::make_shared<tests::empty_io>();

        tests::empty_io* p_raw_io = m_p_io.get();
        std::function<bool()> is_stop_fn = [p_raw_io]()->bool { return p_raw_io->is_stop; };

        m_p_ctx = std::make_unique<details::context>(1, m_p_io, std::make_shared<tests::logger_factory>(), is_stop_fn);
    }

    virtual void TearDown() override {}

    details::context& init(size_t servs_count, bool is_voter = true)
    {
        for (size_t i = 0; i < servs_count; ++i) {
            m_p_io->cluster_cfg.servers.emplace_back(i + 1, std::to_string(i), (i == 0) ? is_voter : true);
        }

        details::utils::init(*m_p_ctx, 1);
        m_p_ctx->election_task = m_p_ctx->p_scheduler->make_task(std::bind(&details::timeout::election_timeout_task, std::ref(*m_p_ctx)));
        m_p_ctx->heartbeat_task = m_p_ctx->p_scheduler->make_task(std::bind(&details::timeout::heartbeat_timeout_task, std::ref(*m_p_ctx)));

        m_p_ctx->p_scheduler->cancel(m_p_ctx->election_task);
        m_p_ctx->p_scheduler->cancel(m_p_ctx->heartbeat_task);

        details::utils::load(*m_p_ctx);
        return *m_p_ctx;
    }

protected:
    tests::empty_io::ptr m_p_io;
    details::context::ptr m_p_ctx;
};

} // <anonymous> namespace

TEST_F(raft_role_convert, become_follower)
{
    details::context& ctx = init(3);

    details::role::become_follower(ctx);
    EXPECT_TRUE(ctx.role.is_follower());
}

TEST_F(raft_role_convert, become_candidate)
{
    details::context& ctx = init(3);

    details::role::become_follower(ctx);
    details::role::become_candidate(ctx);
    EXPECT_TRUE(ctx.role.is_candidate());
}

TEST_F(raft_role_convert, become_candidate_single)
{
    details::context& ctx = init(1);

    details::role::become_follower(ctx);
    details::role::become_candidate(ctx);
    EXPECT_TRUE(ctx.role.is_leader());
}

TEST_F(raft_role_convert, become_leader)
{
    details::context& ctx = init(3);

    details::role::become_follower(ctx);
    details::role::become_candidate(ctx);
    details::role::become_leader(ctx);
    EXPECT_TRUE(ctx.role.is_leader());
}

TEST_F(raft_role_convert, become_leader_non_voters)
{
    details::context& ctx = init(1);

    details::role::become_follower(ctx);
    details::role::become_candidate(ctx);
    EXPECT_TRUE(ctx.role.is_leader());
}

TEST_F(raft_role_convert, update_leader)
{
    details::context& ctx = init(3);

    details::role::become_follower(ctx);
    EXPECT_TRUE(ctx.role.is_follower());
    EXPECT_TRUE(ctx.role.follower_state.leader_id == raft::gk_invalid_id);

    details::role::update_leader(ctx, 3);
    EXPECT_TRUE(ctx.role.is_follower());
    EXPECT_TRUE(ctx.role.follower_state.leader_id == 3);
}

TEST_F(raft_role_convert, update_term)
{
    details::context& ctx = init(3);

    details::role::become_follower(ctx);
    EXPECT_TRUE(ctx.role.is_follower());

    ctx.term = 1;
    details::role::update_term(ctx, 3);
    EXPECT_TRUE(ctx.role.is_follower());
    EXPECT_TRUE(ctx.term == 3);
}

TEST_F(raft_role_convert, update_term_local_term_higher)
{
    details::context& ctx = init(3);

    details::role::become_follower(ctx);
    EXPECT_TRUE(ctx.role.is_follower());

    ctx.term = 3;
    details::role::update_term(ctx, 1);
    EXPECT_TRUE(ctx.role.is_follower());
    EXPECT_TRUE(ctx.term == 3);
}

TEST_F(raft_role_convert, update_term_become_leader)
{
    details::context& ctx = init(3);

    details::role::become_follower(ctx);
    details::role::become_candidate(ctx);
    details::role::become_leader(ctx);
    EXPECT_TRUE(ctx.role.is_leader());

    ctx.term = 1;
    details::role::update_term(ctx, 3);
    EXPECT_TRUE(ctx.role.is_follower());
    EXPECT_TRUE(ctx.term == 3);
}

int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
