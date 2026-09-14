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
#include "raft/details/replication/snapshot.h"
#include "raft/details/role/convert.h"

#include "stub/empty_io.h"
#include "stub/fsm_stub.h"

namespace {

namespace raft = ::wstux::raft;
namespace details = raft::details;
namespace tests = raft::tests;

class raft_snapshot : public ::testing::Test
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

        details::utils::init(*m_p_ctx, m_p_io->cluster_cfg);
        details::utils::load(*m_p_ctx);
        details::role::become_follower(*m_p_ctx);
    }

    virtual void TearDown() override {}

protected:
    tests::empty_io::ptr m_p_io;
    tests::fsm_stub::ptr m_p_fsm;
    details::context::ptr m_p_ctx;
};

bool snapshot_install(raft::details::context& ctx, raft::index_t last_index, raft::term_t last_term,
                      raft::cluster_config conf, raft::index_t conf_index, raft::buffer_type buffer)
{
    raft::details::replication::snapshot::async::install_context::ptr p_async_ctx;
    return raft::details::replication::snapshot::install(ctx, last_index, last_term, std::move(conf), conf_index, std::move(buffer), p_async_ctx);
}

} // <anonymous> namespace

TEST_F(raft_snapshot, install)
{
    details::context& ctx = *m_p_ctx;
    ASSERT_TRUE(ctx.role.is_follower());

    ctx.state.configuration_committed_index = 0;
    ctx.state.configuration_uncommitted_index = 1;
    ctx.state.commit_index = 0;
    ctx.state.last_applied = 0;
    ctx.state.last_stored = 0;
    ctx.log.append_change(1, raft::cluster_config());
    ctx.log.append_change(1, raft::cluster_config());
    ctx.log.append_change(1, raft::cluster_config());

    EXPECT_TRUE(snapshot_install(ctx, 1, 2, raft::cluster_config(), 2, raft::buffer_type()));
    EXPECT_TRUE(ctx.state.configuration_committed_index == 2) << ctx.state.configuration_committed_index;
    EXPECT_TRUE(ctx.state.configuration_uncommitted_index == 0) << ctx.state.configuration_uncommitted_index;
    EXPECT_TRUE(ctx.state.commit_index == 1) << ctx.state.commit_index;
    EXPECT_TRUE(ctx.state.last_applied == 1) << ctx.state.last_applied;
    EXPECT_TRUE(ctx.state.last_stored == 1) << ctx.state.last_stored;
}

TEST_F(raft_snapshot, install_old_snapshot_index)
{
    details::context& ctx = *m_p_ctx;
    ASSERT_TRUE(ctx.role.is_follower());

    ctx.state.configuration_committed_index = 2;
    ctx.state.configuration_uncommitted_index = 0;
    ctx.state.commit_index = 2;
    ctx.state.last_applied = 2;
    ctx.state.last_stored = 2;
    ctx.log.snapshot.last_index = 2;
    ctx.log.append_change(1, raft::cluster_config());
    ctx.log.append_change(1, raft::cluster_config());
    ctx.log.append_change(1, raft::cluster_config());

    EXPECT_TRUE(snapshot_install(ctx, 1, 2, raft::cluster_config(), 2, raft::buffer_type()));
    EXPECT_TRUE(ctx.state.configuration_committed_index == 2) << ctx.state.configuration_committed_index;
    EXPECT_TRUE(ctx.state.configuration_uncommitted_index == 0) << ctx.state.configuration_uncommitted_index;
    EXPECT_TRUE(ctx.state.commit_index == 2) << ctx.state.commit_index;
    EXPECT_TRUE(ctx.state.last_applied == 2) << ctx.state.last_applied;
    EXPECT_TRUE(ctx.state.last_stored == 2) << ctx.state.last_stored;
}

TEST_F(raft_snapshot, install_old_term)
{
    details::context& ctx = *m_p_ctx;
    ASSERT_TRUE(ctx.role.is_follower());

    ctx.state.configuration_committed_index = 0;
    ctx.state.configuration_uncommitted_index = 1;
    ctx.state.commit_index = 0;
    ctx.state.last_applied = 0;
    ctx.state.last_stored = 0;
    ctx.log.append_change(1, raft::cluster_config());
    ctx.log.append_change(1, raft::cluster_config());
    ctx.log.append_change(1, raft::cluster_config());

    EXPECT_TRUE(snapshot_install(ctx, 1, 1, raft::cluster_config(), 2, raft::buffer_type()));
    EXPECT_TRUE(ctx.state.configuration_committed_index == 0) << ctx.state.configuration_committed_index;
    EXPECT_TRUE(ctx.state.configuration_uncommitted_index == 1) << ctx.state.configuration_uncommitted_index;
    EXPECT_TRUE(ctx.state.commit_index == 0) << ctx.state.commit_index;
    EXPECT_TRUE(ctx.state.last_applied == 0) << ctx.state.last_applied;
    EXPECT_TRUE(ctx.state.last_stored == 0) << ctx.state.last_stored;
}

TEST_F(raft_snapshot, install_failed_restore)
{
    details::context& ctx = *m_p_ctx;
    m_p_fsm->is_result = false;
    ASSERT_TRUE(ctx.role.is_follower());

    ctx.state.configuration_committed_index = 2;
    ctx.state.configuration_uncommitted_index = 0;
    ctx.state.commit_index = 2;
    ctx.state.last_applied = 2;
    ctx.state.last_stored = 2;
    ctx.log.append_change(1, raft::cluster_config());
    ctx.log.append_change(1, raft::cluster_config());
    ctx.log.append_change(1, raft::cluster_config());

    EXPECT_FALSE(snapshot_install(ctx, 1, 2, raft::cluster_config(), 2, raft::buffer_type()));
    EXPECT_TRUE(ctx.state.configuration_committed_index == 2) << ctx.state.configuration_committed_index;
    EXPECT_TRUE(ctx.state.configuration_uncommitted_index == 0) << ctx.state.configuration_uncommitted_index;
    EXPECT_TRUE(ctx.state.commit_index == 2) << ctx.state.commit_index;
    EXPECT_TRUE(ctx.state.last_applied == 2) << ctx.state.last_applied;
    EXPECT_TRUE(ctx.state.last_stored == 0) << ctx.state.last_stored;
}

TEST_F(raft_snapshot, install_failed_set_snapshot)
{
    details::context& ctx = *m_p_ctx;
    m_p_io->is_snapshot = false;
    ASSERT_TRUE(ctx.role.is_follower());

    ctx.state.configuration_committed_index = 2;
    ctx.state.configuration_uncommitted_index = 0;
    ctx.state.commit_index = 2;
    ctx.state.last_applied = 2;
    ctx.state.last_stored = 2;
    ctx.log.append_change(1, raft::cluster_config());
    ctx.log.append_change(1, raft::cluster_config());
    ctx.log.append_change(1, raft::cluster_config());

    EXPECT_FALSE(snapshot_install(ctx, 1, 2, raft::cluster_config(), 2, raft::buffer_type()));
    EXPECT_TRUE(ctx.state.configuration_committed_index == 2) << ctx.state.configuration_committed_index;
    EXPECT_TRUE(ctx.state.configuration_uncommitted_index == 0) << ctx.state.configuration_uncommitted_index;
    EXPECT_TRUE(ctx.state.commit_index == 2) << ctx.state.commit_index;
    EXPECT_TRUE(ctx.state.last_applied == 2) << ctx.state.last_applied;
    EXPECT_TRUE(ctx.state.last_stored == 0) << ctx.state.last_stored;
}

TEST_F(raft_snapshot, restore)
{
    details::context& ctx = *m_p_ctx;
    ASSERT_TRUE(ctx.role.is_follower());

    ctx.state.configuration_committed_index = 0;
    ctx.state.configuration_uncommitted_index = 1;
    ctx.state.commit_index = 0;
    ctx.state.last_applied = 0;
    ctx.state.last_stored = 0;

    raft::snapshot sh;
    sh.index = 1;
    sh.conf_index = 2;

    ASSERT_TRUE(raft::details::replication::snapshot::restore(ctx, sh));
    ASSERT_TRUE(ctx.state.configuration_committed_index == 2);
    ASSERT_TRUE(ctx.state.configuration_uncommitted_index == 0);
    ASSERT_TRUE(ctx.state.commit_index == 1);
    ASSERT_TRUE(ctx.state.last_applied == 1);
    ASSERT_TRUE(ctx.state.last_stored == 1);
}

TEST_F(raft_snapshot, restore_failed_fsm_apply)
{
    details::context& ctx = *m_p_ctx;
    m_p_fsm->is_result = false;
    ASSERT_TRUE(ctx.role.is_follower());

    ctx.state.configuration_committed_index = 0;
    ctx.state.configuration_uncommitted_index = 1;
    ctx.state.commit_index = 0;
    ctx.state.last_applied = 0;
    ctx.state.last_stored = 0;

    raft::snapshot sh;
    sh.index = 1;
    sh.conf_index = 2;

    ASSERT_FALSE(raft::details::replication::snapshot::restore(ctx, sh));
    ASSERT_TRUE(ctx.state.configuration_committed_index == 0);
    ASSERT_TRUE(ctx.state.configuration_uncommitted_index == 1);
    ASSERT_TRUE(ctx.state.commit_index == 0);
    ASSERT_TRUE(ctx.state.last_applied == 0);
    ASSERT_TRUE(ctx.state.last_stored == 0);
}

int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
