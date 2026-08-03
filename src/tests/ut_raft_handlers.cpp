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

#include "raft_le/details/handlers/heartbeat_handler.h"
#include "raft_le/details/handlers/timeout_handler.h"
#include "raft_le/details/role/convert.h"

#include "stub/empty_io.h"

namespace {

namespace raft = ::wstux::raft::le;
namespace details = raft::details;
namespace tests = raft::tests;

tests::return_type context(size_t cluster_size = 1, bool is_voter = true)
{
    tests::return_type rt = tests::make_context(cluster_size, is_voter);
    details::utils::init(*rt.first, 1);
    details::utils::load(*rt.first);
    rt.first->p_scheduler->start();
    return rt;
}

} // <anonymous> namespace

TEST(raft_heartbeat_handler, handle_request_invalid_src_id)
{
    using namespace std::chrono_literals;

    tests::return_type rt = context(2);
    details::context::ptr p_ctx = std::move(rt.first);
    tests::empty_io::ptr p_io = rt.second;
    ASSERT_TRUE(p_io->clients.size() == 1) << p_io->clients.size();

    tests::empty_client::ptr p_client = p_io->clients.at(2);
    details::heartbeat::handle_request(*p_ctx, 1, 10, details::heartbeat_message());
    std::this_thread::sleep_for(5ms);
    EXPECT_FALSE(p_client->has_message);
}

TEST(raft_heartbeat_handler, handle_request_invalid_term)
{
    using namespace std::chrono_literals;

    tests::return_type rt = context(2);
    details::context::ptr p_ctx = std::move(rt.first);
    p_ctx->term = 5;
    tests::empty_io::ptr p_io = rt.second;
    ASSERT_TRUE(p_io->clients.size() == 1) << p_io->clients.size();

    tests::empty_client::ptr p_client = p_io->clients.at(2);
    details::heartbeat::handle_request(*p_ctx, 1, 2, details::heartbeat_message());
    std::this_thread::sleep_for(5ms);
    EXPECT_TRUE(p_client->has_message);
}

TEST(raft_heartbeat_handler, handle_request_downgrade_role)
{
    using namespace std::chrono_literals;

    tests::return_type rt = context(2);
    details::context::ptr p_ctx = std::move(rt.first);
    p_ctx->term = 1;
    details::role::become_follower(*p_ctx);
    details::role::become_candidate(*p_ctx);
    EXPECT_TRUE(p_ctx->role.is_candidate());

    details::heartbeat::handle_request(*p_ctx, 1, 2, details::heartbeat_message());
    EXPECT_TRUE(p_ctx->role.is_follower());
}

TEST(raft_heartbeat_handler, handle_response_not_leader)
{
    using namespace std::chrono_literals;

    tests::return_type rt = context(2);
    details::context::ptr p_ctx = std::move(rt.first);
    p_ctx->term = 1;
    ASSERT_TRUE(p_ctx->peers.size() == 1) << p_ctx->peers.size();
    details::peer::ptr p_peer = p_ctx->peers.at(2);
    ASSERT_FALSE(p_peer->recent_recv());

    details::role::become_follower(*p_ctx);
    EXPECT_TRUE(p_ctx->role.is_follower());

    details::heartbeat::handle_response(*p_ctx, 1, 2, details::heartbeat_response_message());
    EXPECT_FALSE(p_peer->recent_recv());
}

TEST(raft_heartbeat_handler, handle_response_local_higher_term)
{
    using namespace std::chrono_literals;

    tests::return_type rt = context(2);
    details::context::ptr p_ctx = std::move(rt.first);
    p_ctx->term = 5;
    ASSERT_TRUE(p_ctx->peers.size() == 1) << p_ctx->peers.size();
    details::peer::ptr p_peer = p_ctx->peers.at(2);
    ASSERT_FALSE(p_peer->recent_recv());

    details::role::become_follower(*p_ctx);
    details::role::become_candidate(*p_ctx);
    details::role::become_leader(*p_ctx);
    EXPECT_TRUE(p_ctx->role.is_leader());

    details::heartbeat::handle_response(*p_ctx, 1, 2, details::heartbeat_response_message());
    EXPECT_FALSE(p_peer->recent_recv());
}

TEST(raft_heartbeat_handler, handle_response_src_higher_term)
{
    using namespace std::chrono_literals;

    tests::return_type rt = context(2);
    details::context::ptr p_ctx = std::move(rt.first);
    p_ctx->term = 1;
    ASSERT_TRUE(p_ctx->peers.size() == 1) << p_ctx->peers.size();
    details::peer::ptr p_peer = p_ctx->peers.at(2);
    ASSERT_FALSE(p_peer->recent_recv());

    details::role::become_follower(*p_ctx);
    details::role::become_candidate(*p_ctx);
    details::role::become_leader(*p_ctx);
    EXPECT_TRUE(p_ctx->role.is_leader());

    details::heartbeat::handle_response(*p_ctx, 5, 2, details::heartbeat_response_message());
    EXPECT_FALSE(p_peer->recent_recv());
    EXPECT_TRUE(p_ctx->role.is_follower()) << (*p_ctx);
    EXPECT_TRUE(p_ctx->term == 5) << p_ctx->term;
}

TEST(raft_heartbeat_handler, handle_response_invalid_peer)
{
    using namespace std::chrono_literals;

    tests::return_type rt = context(2);
    details::context::ptr p_ctx = std::move(rt.first);
    p_ctx->term = 1;
    ASSERT_TRUE(p_ctx->peers.size() == 1) << p_ctx->peers.size();
    details::peer::ptr p_peer = p_ctx->peers.at(2);
    ASSERT_FALSE(p_peer->recent_recv());

    details::role::become_follower(*p_ctx);
    details::role::become_candidate(*p_ctx);
    details::role::become_leader(*p_ctx);
    EXPECT_TRUE(p_ctx->role.is_leader());

    details::heartbeat::handle_response(*p_ctx, 1, 5, details::heartbeat_response_message());
    EXPECT_FALSE(p_peer->recent_recv());
}

TEST(raft_timeout_handler, timeout_stopped_servicce)
{
    tests::return_type rt = context(2);
    details::context::ptr p_ctx = std::move(rt.first);
    details::context& ctx = *p_ctx;
    ctx.election_task = ctx.p_scheduler->make_task(std::bind(&raft::details::timeout::election_timeout_task, std::ref(*p_ctx)));

    details::timeout::election_cancel_task(ctx);
    EXPECT_TRUE(ctx.p_scheduler->is_canceled(ctx.election_task));
    rt.second->is_stop = true;

    details::timeout::election_timeout_task(ctx);
    EXPECT_TRUE(ctx.p_scheduler->is_canceled(ctx.election_task));
}

TEST(raft_timeout_handler, not_quorum)
{
    tests::return_type rt = context(3);
    details::context& ctx = *rt.first;
    details::role::become_follower(ctx);
    details::role::become_candidate(ctx);
    details::role::become_leader(ctx);

    details::timeout::election_timeout_task(ctx);
    EXPECT_TRUE(ctx.role.is_follower());
}

TEST(raft_timeout_handler, election_start)
{
    tests::return_type rt = context(3);
    details::context& ctx = *rt.first;
    ctx.p_scheduler->stop();
    details::role::become_follower(ctx);
    details::role::become_candidate(ctx);
    EXPECT_TRUE(ctx.role.is_candidate());
    ctx.term = 1;
    ctx.role.candidate_state.is_prevote = false;
    EXPECT_TRUE(ctx.term == 1);

    details::timeout::election_timeout_task(ctx);
    EXPECT_TRUE(ctx.role.is_candidate());
    EXPECT_TRUE(ctx.term == 2);
}

int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
