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

#include "stub/empty_io.h"
#include "stub/fsm_stub.h"

namespace {

namespace raft = ::wstux::raft;
namespace details = raft::details;
namespace tests = raft::tests;

class raft_role_convert : public ::testing::Test
{
public:
    virtual void SetUp() override
    {
        m_p_io = std::make_shared<tests::empty_io>();
        m_p_fsm = std::make_shared<tests::fsm_stub>();

        tests::empty_io* p_raw_io = m_p_io.get();
        std::function<bool()> is_stop_fn = [p_raw_io]() -> bool { return p_raw_io->is_stop; };
        std::function<void()> stop_fn = []() -> void {};

        m_p_ctx = std::make_unique<details::context>(1, m_p_io, m_p_fsm, raft::logging_handler::ptr(), is_stop_fn, stop_fn);
    }

    virtual void TearDown() override {}

    details::context& init(size_t servs_count, bool is_voter = true)
    {
        for (size_t i = 0; i < servs_count; ++i) {
            m_p_io->cluster_cfg.servers.emplace_back(i + 1, std::to_string(i + 1), (i == 0) ? is_voter : true);
        }

        details::utils::bootstrap(*m_p_ctx, m_p_io->cluster_cfg);
        details::utils::init(*m_p_ctx, m_p_io->cfg);
        m_p_ctx->election_task = m_p_ctx->schd.make_task(std::bind(&details::timeout::election_timeout_task, std::ref(*m_p_ctx)));
        m_p_ctx->heartbeat_task = m_p_ctx->schd.make_task(std::bind(&details::timeout::heartbeat_timeout_task, std::ref(*m_p_ctx)));

        m_p_ctx->schd.cancel(m_p_ctx->election_task);
        m_p_ctx->schd.cancel(m_p_ctx->heartbeat_task);

        details::utils::load(*m_p_ctx);
        return *m_p_ctx;
    }

protected:
    tests::empty_io::ptr m_p_io;
    tests::fsm_stub::ptr m_p_fsm;
    details::context::ptr m_p_ctx;
};

} // <anonymous> namespace

/**
 *  \test   Verification of node transition to the follower role.
 *
 *  **Test logic description:**
 *  Verifies that calling the role conversion function changes the current state
 *  of the Raft context to the follower role.
 *
 *  **Steps to reproduce:**
 *  -# Initialize the Raft context with a cluster size of 3 nodes.
 *  -# Force the role transition by calling the become_follower function.
 *  -# Verify that the context state reflects the follower role.
 *
 *  \expected_result    The node successfully switches its role, and the is_follower
 *      check returns true.
 */
TEST_F(raft_role_convert, become_follower)
{
    details::context& ctx = init(3);

    details::role::become_follower(ctx);
    EXPECT_TRUE(ctx.role.is_follower());
}

/**
 *  \test   Verification of node transition to the candidate role in a multi-node cluster.
 *
 *  **Test logic description:**
 *  Verifies that a node in a standard cluster (3 nodes) correctly transitions
 *  from a follower to a candidate state during an election phase.
 *
 *  **Steps to reproduce:**
 *  -# Initialize the Raft context with a cluster size of 3 nodes.
 *  -# Transition the node to the follower role.
 *  -# Initiate a transition to the candidate role.
 *  -# Verify that the context state reflects the candidate role.
 *
 *  \expected_result    The node successfully switches to the candidate role,
 *      and the is_candidate check returns true.
 */
TEST_F(raft_role_convert, become_candidate)
{
    details::context& ctx = init(3);

    details::role::become_follower(ctx);
    details::role::become_candidate(ctx);
    EXPECT_TRUE(ctx.role.is_candidate());
}

/**
 *  \test   Verification of single-node cluster transition to the leader role.
 *
 *  **Test logic description:**
 *  Verifies that in a single-node cluster, a node attempting to become a candidate
 *  immediately becomes the leader, as it automatically wins the majority vote.
 *
 *  **Steps to reproduce:**
 *  -# Initialize the Raft context with a cluster size of 1 node.
 *  -# Transition the node to the follower role.
 *  -# Attempt a transition to the candidate role.
 *  -# Verify that the context state immediately reflects the leader role.
 *
 *  \expected_result    The single node bypasses the candidate election phase
 *      and instantly becomes the leader. The is_leader check returns true.
 */
TEST_F(raft_role_convert, become_candidate_single)
{
    details::context& ctx = init(1);

    details::role::become_follower(ctx);
    details::role::become_candidate(ctx);
    EXPECT_TRUE(ctx.role.is_leader());
}

/**
 *  \test   Verification of node transition to the leader role.
 *
 *  **Test logic description:**
 *  Verifies the full sequence of role transitions in a multi-node cluster,
 *  ensuring that a candidate node can successfully become a leader.
 *
 *  **Steps to reproduce:**
 *  -# Initialize the Raft context with a cluster size of 3 nodes.
 *  -# Transfer the node to the follower state.
 *  -# Initiate an election by changing the role to candidate.
 *  -# Explicitly elevate the node's role to leader.
 *  -# Verify that the context state reflects the leader role.
 *
 *  \expected_result    The node sequentially switches through follower and candidate
 *      states, successfully becomes the leader, and the is_leader check returns true.
 */
TEST_F(raft_role_convert, become_leader)
{
    details::context& ctx = init(3);

    details::role::become_follower(ctx);
    details::role::become_candidate(ctx);
    details::role::become_leader(ctx);
    EXPECT_TRUE(ctx.role.is_leader());
}

/**
 *  \test   Verification of automatic leader election in a single-node cluster.
 *
 *  **Test logic description:**
 *  Verifies that in a cluster consisting of only one node, the transition to
 *  a candidate immediately promotes the node to a leader (since a majority is
 *  already reached).
 *
 *  **Steps to reproduce:**
 *  -# Initialize the Raft context with a cluster size of 1 node.
 *  -# Transfer the node to the follower state.
 *  -# Trigger the transition to candidate.
 *  -# Verify that the node has automatically bypassed the election and became the leader.
 *
 *  \expected_result    The candidate role transition triggers an immediate win
 *      in a single-node setup, and the is_leader check returns true.
 */
TEST_F(raft_role_convert, become_leader_non_voters)
{
    details::context& ctx = init(1);

    details::role::become_follower(ctx);
    details::role::become_candidate(ctx);
    EXPECT_TRUE(ctx.role.is_leader());
}
/**
 *  \test   Verification of immediate log commitment upon becoming leader in a
 *      single-voter cluster.
 *
 *  **Test logic description:**
 *  Verifies that when a node becomes a leader in a configuration with only 1 voting member,
 *  and it has uncommitted stored entries (last_stored > commit_index), it immediately
 *  advances its commit_index to match last_stored and triggers entries replication.
 *
 *  **Steps to reproduce:**
 *  -# Initialize the Raft context with a single-node configuration (voters_count == 1).
 *  -# Transition the node to the follower state, then to the candidate state.
 *  -# Emulate uncommitted entries by setting ctx.state.last_stored to a value higher than ctx.state.commit_index.
 *  -# Invoke become_leader to trigger the single-voter optimization branch.
 *  -# Verify that the node successfully becomes the leader.
 *  -# Verify that ctx.state.commit_index has been advanced to match ctx.state.last_stored.
 *
 *  \expected_result    The leader immediately commits its local uncommitted
 *      entries, updating commit_index to last_stored, and initiates the
 *      replication commit process.
 */
TEST_F(raft_role_convert, become_leader_single_commit)
{
    details::context& ctx = init(1);

    details::role::become_follower(ctx);
    ctx.state.commit_index = 5;
    ctx.state.last_stored = 10;
    ASSERT_TRUE(ctx.role.is_follower());

    details::role::become_candidate(ctx);
    ASSERT_TRUE(ctx.role.is_leader());
    EXPECT_TRUE(ctx.state.commit_index == ctx.state.last_stored) << ctx.state.commit_index << " != " << ctx.state.last_stored;
    EXPECT_TRUE(ctx.state.commit_index == 10) << ctx.state.commit_index << " != 10";
}
/**
 *  \test   Verification of leader ID updates within the follower role.
 *
 *  **Test logic description:**
 *  Verifies that a follower node correctly updates and tracks the active
 *  leader's identifier upon receiving a corresponding update command.
 *
 *  **Steps to reproduce:**
 *  -# Initialize the Raft context with a cluster size of 3 nodes.
 *  -# Transfer the node to the follower state.
 *  -# Verify that the node is a follower and its initial leader ID is invalid.
 *  -# Update the current leader info, specifying node ID 3 as the leader.
 *  -# Verify that the node remains a follower and its leader ID is now updated to 3.
 *
 *  \expected_result    The follower node retains its follower role during the
 *      update, and the tracked leader ID matches the newly provided value.
 */
TEST_F(raft_role_convert, update_leader)
{
    details::context& ctx = init(3);

    details::role::become_follower(ctx);
    EXPECT_TRUE(ctx.role.is_follower());
    EXPECT_TRUE(ctx.role.leader_id == raft::gk_invalid_id);

    details::role::update_leader(ctx, 3);
    EXPECT_TRUE(ctx.role.is_follower());
    EXPECT_TRUE(ctx.role.leader_id == 3);
}

/**
 *  \test   Verification of term update when a higher term is received.
 *
 *  **Test logic description:**
 *  Verifies that the node correctly updates its current term when it encounters
 *  a higher term from the cluster, while maintaining its follower role.
 *
 *  **Steps to reproduce:**
 *  -# Initialize the Raft context with a cluster size of 3 nodes.
 *  -# Transfer the node to the follower state and verify the role.
 *  -# Explicitly set the local term to 1.
 *  -# Invoke update_term with a higher term value of 3.
 *  -# Verify that the node remains a follower and its local term is updated to 3.
 *
 *  \expected_result    The node successfully updates its local term to match
 *      the higher external term and preserves its follower status.
 */
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

/**
 *  \test   Verification that local term is preserved if the external term is lower.
 *
 *  **Test logic description:**
 *  Verifies that the node ignores term updates and retains its current term
 *  if the incoming external term is lower than the locally stored term.
 *
 *  **Steps to reproduce:**
 *  -# Initialize the Raft context with a cluster size of 3 nodes.
 *  -# Transfer the node to the follower state and verify the role.
 *  -# Explicitly set the local term to a higher value of 3.
 *  -# Invoke update_term with a lower term value of 1.
 *  -# Verify that the node remains a follower and its local term is unchanged (stays 3).
 *
 *  \expected_result    The local term is not overwritten by a lower external term,
 *      and the follower status remains unaffected.
 */
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

/**
 *  \test   Verification of leader stepping down to follower when a higher term
 *      is discovered.
 *
 *  **Test logic description:**
 *  Verifies that a leader node immediately steps down to the follower role and
 *  updates its term if it discovers a cluster term higher than its own.
 *
 *  **Steps to reproduce:**
 *  -# Initialize the Raft context with a cluster size of 3 nodes.
 *  -# Transition the node sequentially through follower and candidate states to leader.
 *  -# Verify that the node has successfully assumed the leader role.
 *  -# Explicitly set the local leader term to 1.
 *  -# Invoke update_term with a higher external term value of 3.
 *  -# Verify that the node reverts to the follower role and its term updates to 3.
 *
 *  \expected_result    The leader node steps down to become a follower upon
 *      encountering a higher term, and its local term correctly synchronizes
 *      to 3.
 */
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
