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

#include "raft/details/connection/serialization.h"
#include "raft/details/handlers/heartbeat_handler.h"
#include "raft/details/handlers/timeout_handler.h"
#include "raft/details/handlers/vote_handler.h"
#include "raft/details/role/convert.h"

#include "stub/empty_io.h"

namespace {

namespace raft = ::wstux::raft;
namespace details = raft::details;
namespace tests = raft::tests;

class raft_handler : public ::testing::Test
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

        details::utils::init(*m_p_ctx);
        details::utils::load(*m_p_ctx);
        m_p_ctx->schd.start();
        return *m_p_ctx;
    }

protected:
    tests::empty_io::ptr m_p_io;
    details::context::ptr m_p_ctx;
};

using raft_heartbeat_handler = raft_handler;
using raft_timeout_handler = raft_handler;
using raft_vote_handler = raft_handler;

} // <anonymous> namespace

TEST_F(raft_heartbeat_handler, handle_request_invalid_src_id)
{
    using namespace std::chrono_literals;

    details::context& ctx = init(2);
    ASSERT_TRUE(m_p_io->clients.size() == 1) << m_p_io->clients.size();

    tests::empty_client::ptr p_client = m_p_io->clients.at(2);
    details::heartbeat::handle_request(ctx, 1, 10, details::heartbeat_message());
    std::this_thread::sleep_for(5ms);
    details::message msg = details::deserialize(p_client->buffer);
    EXPECT_TRUE(msg.type == details::message_type::invalid);
}

TEST_F(raft_heartbeat_handler, handle_request_invalid_term)
{
    using namespace std::chrono_literals;

    details::context& ctx = init(2);
    ctx.term = 5;
    ASSERT_TRUE(m_p_io->clients.size() == 1) << m_p_io->clients.size();

    tests::empty_client::ptr p_client = m_p_io->clients.at(2);
    details::heartbeat::handle_request(ctx, 1, 2, details::heartbeat_message());
    std::this_thread::sleep_for(5ms);
    details::message msg = details::deserialize(p_client->buffer);
    EXPECT_TRUE(msg.type == details::message_type::heartbeat_response);
    EXPECT_FALSE(msg.heartbeat_resp.accept);
}

TEST_F(raft_heartbeat_handler, handle_request_downgrade_role)
{
    using namespace std::chrono_literals;

    details::context& ctx = init(2);
    ctx.term = 1;
    details::role::become_follower(ctx);
    details::role::become_candidate(ctx);
    EXPECT_TRUE(ctx.role.is_candidate());

    details::heartbeat::handle_request(ctx, 1, 2, details::heartbeat_message());
    EXPECT_TRUE(ctx.role.is_follower());
}

TEST_F(raft_heartbeat_handler, handle_response_not_leader)
{
    using namespace std::chrono_literals;

    details::context& ctx = init(2);
    ctx.term = 1;
    ASSERT_TRUE(ctx.peers.size() == 1) << ctx.peers.size();
    details::peer::ptr p_peer = details::peers::find(ctx, 2);
    ASSERT_FALSE(p_peer->recent_recv);

    details::role::become_follower(ctx);
    EXPECT_TRUE(ctx.role.is_follower());

    details::heartbeat::handle_response(ctx, 1, 2, details::heartbeat_response_message());
    EXPECT_FALSE(p_peer->recent_recv);
}

TEST_F(raft_heartbeat_handler, handle_response_local_higher_term)
{
    using namespace std::chrono_literals;

    details::context& ctx = init(2);
    ctx.term = 5;
    ASSERT_TRUE(ctx.peers.size() == 1) << ctx.peers.size();
    details::peer::ptr p_peer = details::peers::find(ctx, 2);
    ASSERT_FALSE(p_peer->recent_recv);

    details::role::become_follower(ctx);
    details::role::become_candidate(ctx);
    details::role::become_leader(ctx);
    EXPECT_TRUE(ctx.role.is_leader());

    details::heartbeat::handle_response(ctx, 1, 2, details::heartbeat_response_message());
    EXPECT_FALSE(p_peer->recent_recv);
}

TEST_F(raft_heartbeat_handler, handle_response_src_higher_term)
{
    using namespace std::chrono_literals;

    details::context& ctx = init(2);
    ctx.term = 1;
    ASSERT_TRUE(ctx.peers.size() == 1) << ctx.peers.size();
    details::peer::ptr p_peer = details::peers::find(ctx, 2);
    ASSERT_FALSE(p_peer->recent_recv);

    details::role::become_follower(ctx);
    details::role::become_candidate(ctx);
    details::role::become_leader(ctx);
    EXPECT_TRUE(ctx.role.is_leader());

    details::heartbeat::handle_response(ctx, 5, 2, details::heartbeat_response_message());
    EXPECT_FALSE(p_peer->recent_recv);
    EXPECT_TRUE(ctx.role.is_follower()) << ctx;
    EXPECT_TRUE(ctx.term == 5) << ctx.term;
}

TEST_F(raft_heartbeat_handler, handle_response_invalid_peer)
{
    using namespace std::chrono_literals;

    details::context& ctx = init(2);
    ctx.term = 1;
    ASSERT_TRUE(ctx.peers.size() == 1) << ctx.peers.size();
    details::peer::ptr p_peer = details::peers::find(ctx, 2);
    ASSERT_FALSE(p_peer->recent_recv);

    details::role::become_follower(ctx);
    details::role::become_candidate(ctx);
    details::role::become_leader(ctx);
    EXPECT_TRUE(ctx.role.is_leader());

    details::heartbeat::handle_response(ctx, 1, 5, details::heartbeat_response_message());
    EXPECT_FALSE(p_peer->recent_recv);
}

TEST_F(raft_timeout_handler, timeout_stopped_servicce)
{
    details::context& ctx = init(2);
    ctx.election_task = ctx.schd.make_task(std::bind(&raft::details::timeout::election_timeout_task, std::ref(ctx)));

    details::timeout::election_cancel_task(ctx);
    EXPECT_TRUE(ctx.schd.is_canceled(ctx.election_task));
    m_p_io->is_stop = true;

    details::timeout::election_timeout_task(ctx);
    EXPECT_TRUE(ctx.schd.is_canceled(ctx.election_task));
}

TEST_F(raft_timeout_handler, not_quorum)
{
    details::context& ctx = init(3);
    details::role::become_follower(ctx);
    details::role::become_candidate(ctx);
    details::role::become_leader(ctx);

    details::timeout::election_timeout_task(ctx);
    EXPECT_TRUE(ctx.role.is_follower());
}

TEST_F(raft_timeout_handler, election_start)
{
    details::context& ctx = init(3);
    ctx.schd.stop();
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

TEST_F(raft_vote_handler, handle_request_invalid_src_id)
{
    using namespace std::chrono_literals;

    details::context& ctx = init(2);
    ASSERT_TRUE(m_p_io->clients.size() == 1) << m_p_io->clients.size();

    tests::empty_client::ptr p_client = m_p_io->clients.at(2);
    details::vote::handle_request(ctx, 1, 10, details::vote_message());
    std::this_thread::sleep_for(5ms);
    details::message msg = details::deserialize(p_client->buffer);
    EXPECT_TRUE(msg.type == details::message_type::invalid);
}

TEST_F(raft_vote_handler, handle_request_invalid_term)
{
    using namespace std::chrono_literals;

    details::context& ctx = init(2);
    ctx.term = 5;
    ASSERT_TRUE(m_p_io->clients.size() == 1) << m_p_io->clients.size();

    tests::empty_client::ptr p_client = m_p_io->clients.at(2);
    details::vote::handle_request(ctx, 1, 2, details::vote_message());
    std::this_thread::sleep_for(5ms);
    details::message msg = details::deserialize(p_client->buffer);
    EXPECT_TRUE(msg.type == details::message_type::vote_response);
    EXPECT_FALSE(msg.vote_resp.accept);
}

TEST_F(raft_vote_handler, handle_request_not_voter)
{
    using namespace std::chrono_literals;

    details::context& ctx = init(2);
    ctx.term = 1;
    ctx.role.is_voter = false;
    ASSERT_TRUE(m_p_io->clients.size() == 1) << m_p_io->clients.size();

    tests::empty_client::ptr p_client = m_p_io->clients.at(2);
    details::vote::handle_request(ctx, 1, 2, details::vote_message());
    std::this_thread::sleep_for(5ms);
    details::message msg = details::deserialize(p_client->buffer);
    EXPECT_TRUE(msg.type == details::message_type::vote_response);
    EXPECT_FALSE(msg.vote_resp.accept);
}

TEST_F(raft_vote_handler, handle_request_occupied_vote)
{
    using namespace std::chrono_literals;

    details::context& ctx = init(2);
    ctx.term = 1;
    ASSERT_TRUE(m_p_io->clients.size() == 1) << m_p_io->clients.size();

    tests::empty_client::ptr p_client = m_p_io->clients.at(2);
    details::vote_message msg;
    msg.is_prevote = false;
    ctx.role.voted_for = 7;
    details::vote::handle_request(ctx, 1, 2, msg);
    std::this_thread::sleep_for(5ms);
    details::message dst_msg = details::deserialize(p_client->buffer);
    EXPECT_TRUE(dst_msg.type == details::message_type::vote_response);
    EXPECT_FALSE(dst_msg.vote_resp.accept);
}

TEST_F(raft_vote_handler, handle_response_invalid_src_id)
{
    details::context& ctx = init(2);
    details::role::become_follower(ctx);
    details::role::become_candidate(ctx);
    ctx.role.candidate_state.is_prevote = true;
    ctx.role.candidate_state.votes_granted = 0;

    details::vote_response_message msg;
    msg.is_prevote = true;
    msg.accept = true;
    details::vote::handle_response(ctx, 1, 10, msg);
    EXPECT_TRUE(ctx.role.candidate_state.votes_granted == 0);
}

TEST_F(raft_vote_handler, handle_response_invalid_term)
{
    using namespace std::chrono_literals;

    details::context& ctx = init(2);
    details::role::become_follower(ctx);
    details::role::become_candidate(ctx);
    ctx.term = 5;
    ctx.role.candidate_state.is_prevote = true;
    ctx.role.candidate_state.votes_granted = 0;

    details::vote_response_message msg;
    msg.is_prevote = true;
    msg.accept = true;
    details::vote::handle_response(ctx, 1, 2, msg);
    EXPECT_TRUE(ctx.role.candidate_state.votes_granted == 0);
}

TEST_F(raft_vote_handler, handle_response_outdated_term)
{
    using namespace std::chrono_literals;

    details::context& ctx = init(2);
    details::role::become_follower(ctx);
    details::role::become_candidate(ctx);
    ctx.term = 1;
    ctx.role.candidate_state.is_prevote = false;
    ctx.role.candidate_state.votes_granted = 0;

    details::vote_response_message msg;
    msg.is_prevote = false;
    msg.accept = true;
    details::vote::handle_response(ctx, 5, 2, msg);
    EXPECT_TRUE(ctx.role.is_follower());
}

TEST_F(raft_vote_handler, handle_response_outdated_prevote_term)
{
    using namespace std::chrono_literals;

    details::context& ctx = init(2);
    details::role::become_follower(ctx);
    details::role::become_candidate(ctx);
    ctx.term = 1;
    ctx.role.candidate_state.is_prevote = true;
    ctx.role.candidate_state.votes_granted = 0;

    details::vote_response_message msg;
    msg.is_prevote = true;
    msg.accept = false;
    details::vote::handle_response(ctx, 5, 2, msg);
    EXPECT_TRUE(ctx.role.is_follower());
}

int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
