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
#include "raft/details/connection/serialization.h"
#include "raft/details/handlers/timeout_handler.h"
#include "raft/details/replication/membership.h"
#include "raft/details/role/convert.h"

#include "stub/empty_io.h"
#include "stub/fsm_stub.h"

namespace {

namespace raft = ::wstux::raft;
namespace details = raft::details;
namespace tests = raft::tests;

class raft_membership : public ::testing::Test
{
public:
    virtual void SetUp() override
    {
        m_p_io = std::make_shared<tests::empty_io>();
        m_p_fsm = std::make_shared<tests::fsm_stub>();

        tests::empty_io* p_raw_io = m_p_io.get();
        std::function<bool()> is_stop_fn = [p_raw_io]()->bool { return p_raw_io->is_stop; };

        m_p_ctx = std::make_unique<details::context>(1, m_p_io, m_p_fsm, raft::logging_handler::ptr(), is_stop_fn);

        m_p_io->cluster_cfg.servers.emplace_back(1, std::to_string(1), true);
        m_p_io->cluster_cfg.servers.emplace_back(2, std::to_string(2), true);
        m_p_io->cluster_cfg.servers.emplace_back(3, std::to_string(3), true);

        details::utils::bootstrap(*m_p_ctx, m_p_io->cluster_cfg);
        details::utils::init(*m_p_ctx);
        details::utils::load(*m_p_ctx);
        details::role::become_follower(*m_p_ctx);
    }

    virtual void TearDown() override {}

protected:
    tests::empty_io::ptr m_p_io;
    tests::fsm_stub::ptr m_p_fsm;
    details::context::ptr m_p_ctx;
};

} // <anonymous> namespace

namespace wstux {
namespace raft {

bool operator==(const server_config& lhs, const server_config& rhs)
{
    bool is_eq = true;
    is_eq = is_eq && (lhs.id == rhs.id);
    is_eq = is_eq && (lhs.address == rhs.address);
    is_eq = is_eq && (lhs.is_voter == rhs.is_voter);
    return is_eq;
}

} // namespace raft
} // namespace wstux

TEST_F(raft_membership, append)
{
    details::context& ctx = *m_p_ctx;
    ASSERT_TRUE(ctx.role.is_follower());
    details::role::become_candidate(ctx);
    details::role::become_leader(ctx);
    ASSERT_TRUE(ctx.role.is_leader());

    raft::cluster_config cluster_cfg = ctx.state.cluster_cfg;
    ASSERT_TRUE(cluster_cfg.servers.size() == 3);
    cluster_cfg.servers.emplace_back(5, std::to_string(5), true);

    ASSERT_TRUE(ctx.role.leader.peers.size() == 2);
    ASSERT_TRUE(details::replication::membership::append(ctx, raft::server_config({5, std::to_string(5), true})));
    ASSERT_TRUE(cluster_cfg.servers == ctx.state.cluster_cfg.servers);
    ASSERT_TRUE(ctx.role.leader.peers.size() == 3);
    ASSERT_TRUE(details::utils::is_in_cluster(ctx, 2));
    ASSERT_TRUE(details::utils::is_in_cluster(ctx, 3));
    ASSERT_TRUE(details::utils::is_in_cluster(ctx, 5));
}

TEST_F(raft_membership, append_existing_member)
{
    details::context& ctx = *m_p_ctx;
    ASSERT_TRUE(ctx.role.is_follower());

    const raft::cluster_config cluster_cfg = ctx.state.cluster_cfg;
    ASSERT_TRUE(cluster_cfg.servers.size() == 3);
    ASSERT_FALSE(details::replication::membership::append(ctx, raft::server_config({2, std::to_string(2), true})));
    ASSERT_TRUE(cluster_cfg.servers == ctx.state.cluster_cfg.servers);
}

TEST_F(raft_membership, DISABLED_append_failed_to_append_changes)
{
    details::context& ctx = *m_p_ctx;
    ASSERT_TRUE(ctx.role.is_follower());
    m_p_io->is_append = false;

    const raft::cluster_config cluster_cfg = ctx.state.cluster_cfg;
    ASSERT_TRUE(cluster_cfg.servers.size() == 3);
    ASSERT_FALSE(details::replication::membership::append(ctx, raft::server_config({5, std::to_string(5), true})));
    ASSERT_TRUE(cluster_cfg.servers == ctx.state.cluster_cfg.servers);
}

TEST_F(raft_membership, remove)
{
    details::context& ctx = *m_p_ctx;
    ASSERT_TRUE(ctx.role.is_follower());
    ASSERT_TRUE(ctx.role.is_follower());
    details::role::become_candidate(ctx);
    details::role::become_leader(ctx);
    ASSERT_TRUE(ctx.role.is_leader());

    ASSERT_TRUE(ctx.role.leader.peers.size() == 2);
    ASSERT_TRUE(details::replication::membership::remove(ctx, 2));
    ASSERT_TRUE(ctx.role.leader.peers.size() == 1);
    ASSERT_TRUE(details::utils::is_in_cluster(ctx, 3));
}

TEST_F(raft_membership, remove_not_existing_member)
{
    details::context& ctx = *m_p_ctx;
    ASSERT_TRUE(ctx.role.is_follower());

    const raft::cluster_config cluster_cfg = ctx.state.cluster_cfg;
    ASSERT_TRUE(cluster_cfg.servers.size() == 3);
    ASSERT_FALSE(details::replication::membership::remove(ctx, 7));
    ASSERT_TRUE(cluster_cfg.servers == ctx.state.cluster_cfg.servers);
}

TEST_F(raft_membership, update)
{
    details::context& ctx = *m_p_ctx;
    ASSERT_TRUE(ctx.role.is_follower());

    raft::cluster_config cfg;
    cfg.servers.emplace_back(1, std::to_string(1), true);
    cfg.servers.emplace_back(2, std::to_string(2), true);
    cfg.servers.emplace_back(3, std::to_string(3), true);
    cfg.servers.emplace_back(4, std::to_string(4), true);

    raft::entry::ptr p_entry = std::make_shared<raft::entry>();
    p_entry->term = 1;
    p_entry->type = raft::entry_type::change;
    p_entry->buffer = details::serialize(cfg);

    ASSERT_TRUE(ctx.state.cluster_cfg.servers.size() == 3);
    ASSERT_TRUE(details::replication::membership::update(ctx, p_entry));
    ASSERT_TRUE(cfg.servers == ctx.state.cluster_cfg.servers);
}

TEST_F(raft_membership, update_invalid_entry)
{
    details::context& ctx = *m_p_ctx;
    ASSERT_TRUE(ctx.role.is_follower());

    raft::entry::ptr p_entry = std::make_shared<raft::entry>();
    p_entry->term = 1;
    p_entry->type = raft::entry_type::command;

    const raft::cluster_config cluster_cfg = ctx.state.cluster_cfg;
    ASSERT_TRUE(cluster_cfg.servers.size() == 3);
    ASSERT_FALSE(details::replication::membership::update(ctx, p_entry));
    ASSERT_TRUE(cluster_cfg.servers == ctx.state.cluster_cfg.servers);
}

TEST_F(raft_membership, update_invalid_configuration)
{
    details::context& ctx = *m_p_ctx;
    ASSERT_TRUE(ctx.role.is_follower());

    raft::cluster_config cfg;
    cfg.servers.emplace_back(1, std::to_string(1), true);
    cfg.servers.emplace_back(2, std::to_string(2), true);
    cfg.servers.emplace_back(3, std::to_string(3), true);
    cfg.servers.emplace_back(3, std::to_string(3), true);

    raft::entry::ptr p_entry = std::make_shared<raft::entry>();
    p_entry->term = 1;
    p_entry->type = raft::entry_type::change;
    p_entry->buffer = details::serialize(cfg);

    const raft::cluster_config cluster_cfg = ctx.state.cluster_cfg;
    ASSERT_TRUE(cluster_cfg.servers.size() == 3);
    ASSERT_FALSE(details::replication::membership::update(ctx, p_entry));
    ASSERT_TRUE(cluster_cfg.servers == ctx.state.cluster_cfg.servers);
}

TEST_F(raft_membership, DISABLED_update_dawngrade_to_follover)
{
    details::context& ctx = *m_p_ctx;
    ASSERT_TRUE(ctx.role.is_follower());
    details::role::become_candidate(ctx);
    details::role::become_leader(ctx);
    ASSERT_TRUE(ctx.role.is_leader());

    raft::cluster_config cfg;
    cfg.servers.emplace_back(2, std::to_string(2), true);
    cfg.servers.emplace_back(3, std::to_string(3), true);
    cfg.servers.emplace_back(4, std::to_string(4), true);

    raft::entry::ptr p_entry = std::make_shared<raft::entry>();
    p_entry->term = 1;
    p_entry->type = raft::entry_type::change;
    p_entry->buffer = details::serialize(cfg);

    ASSERT_TRUE(ctx.role.leader.peers.size() == 2);
    ASSERT_TRUE(details::replication::membership::update(ctx, p_entry));
    ASSERT_TRUE(ctx.role.is_follower());
    ASSERT_TRUE(ctx.role.leader.peers.size() == 3);
    ASSERT_TRUE(details::utils::is_in_cluster(ctx, 2));
    ASSERT_TRUE(details::utils::is_in_cluster(ctx, 3));
    ASSERT_TRUE(details::utils::is_in_cluster(ctx, 4));
    ASSERT_TRUE(cfg.servers == ctx.state.cluster_cfg.servers);
}

int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
