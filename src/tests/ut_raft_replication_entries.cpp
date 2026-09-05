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
#include "raft/details/replication/entries.h"
#include "raft/details/role/convert.h"

#include "stub/empty_io.h"
#include "stub/fsm_stub.h"

namespace {

namespace raft = ::wstux::raft;
namespace details = raft::details;
namespace tests = raft::tests;

class raft_entries : public ::testing::Test
{
public:
    virtual void SetUp() override{
        m_p_io = std::make_shared<tests::empty_io>();
        m_p_fsm = std::make_shared<tests::fsm_stub>();

        tests::empty_io* p_raw_io = m_p_io.get();
        std::function<bool()> is_stop_fn = [p_raw_io]()->bool { return p_raw_io->is_stop; };

        m_p_ctx = std::make_unique<details::context>(1, m_p_io, m_p_fsm, raft::logging_handler::ptr(), is_stop_fn);

        m_p_io->cluster_cfg.servers.emplace_back(1, std::to_string(1), true);

        details::utils::init(*m_p_ctx);
        details::utils::load(*m_p_ctx);
        details::role::become_follower(*m_p_ctx);
    }

    virtual void TearDown() override {}

    static raft::entry::list make_entries(raft::term_t eterm, raft::entry_type etype, const size_t cluster_size = 1)
    {
        raft::entry::list entries;
        entries.push_back(std::make_shared<raft::entry>());
        entries.back()->term = eterm;
        entries.back()->type = etype;
        if (etype == raft::entry_type::change) {
            raft::cluster_config cfg;
            for (size_t i = 0; i < cluster_size; ++i) {
                cfg.servers.emplace_back(i +1, std::to_string(i + 1), true);
            }
            entries.back()->buffer = details::serialize(cfg);
        }
        return entries;
    }

protected:
    tests::empty_io::ptr m_p_io;
    tests::fsm_stub::ptr m_p_fsm;
    details::context::ptr m_p_ctx;
};

bool entries_append(details::context& ctx, raft::term_t term, raft::index_t leader_commit, raft::index_t prev_log_index,
                    raft::term_t prev_log_term, const raft::entry::list& entries)
{
    details::replication::entries::async::append_context::ptr p_async_ctx;
    return details::replication::entries::append(ctx, term, leader_commit, prev_log_index, prev_log_term, entries, p_async_ctx);
}

bool entries_apply_command(details::context& ctx, raft::buffer_type buf)
{
    details::replication::entries::async::apply_context::ptr p_async_ctx;
    return details::replication::entries::apply_command(ctx, std::move(buf), p_async_ctx);
}

bool entries_apply_configuration(details::context& ctx, raft::cluster_config cfg)
{
    details::replication::entries::async::apply_context::ptr p_async_ctx;
    return details::replication::entries::apply_configuration(ctx, std::move(cfg), p_async_ctx);
}

} // <anonymous> namespace

TEST_F(raft_entries, append)
{
    details::context& ctx = *m_p_ctx;
    ASSERT_TRUE(ctx.role.is_follower());

    ctx.term = 1;

    raft::entry::list entries = raft_entries::make_entries(1, raft::entry_type::change);
    ASSERT_TRUE(entries_append(ctx, 1, 1, 0, 1, entries));
}

TEST_F(raft_entries, append_invalid_cconfiguration)
{
    details::context& ctx = *m_p_ctx;
    ASSERT_TRUE(ctx.role.is_follower());

    ctx.term = 1;

    raft::entry::list entries = raft_entries::make_entries(1, raft::entry_type::change, 0);
    ASSERT_TRUE(entries_append(ctx, 1, 1, 0, 1, entries));
}

TEST_F(raft_entries, append_non_empty_log)
{
    details::context& ctx = *m_p_ctx;
    ASSERT_TRUE(ctx.role.is_follower());

    ctx.term = 1;
    ctx.state.commit_index = 1;
    ctx.log.append_command(1, raft::buffer_type());

    raft::entry::list entries = raft_entries::make_entries(1, raft::entry_type::change);
    ASSERT_TRUE(entries_append(ctx, 1, 1, 1, 1, entries));
}

TEST_F(raft_entries, append_resolve_conflicts)
{
    details::context& ctx = *m_p_ctx;
    ASSERT_TRUE(ctx.role.is_follower());

    ctx.term = 1;
    ctx.state.commit_index = 2;
    ctx.state.configuration_uncommitted_index = 0;
    ctx.log.append_command(2, raft::buffer_type());
    ctx.log.append_command(2, raft::buffer_type());

    raft::entry::list entries = raft_entries::make_entries(1, raft::entry_type::change);
    ASSERT_TRUE(entries_append(ctx, 1, 1, 1, 2, entries));
}

TEST_F(raft_entries, append_resolve_conflicts_fix_last_stored)
{
    details::context& ctx = *m_p_ctx;
    ASSERT_TRUE(ctx.role.is_follower());

    ctx.term = 1;
    ctx.state.commit_index = 2;
    ctx.state.last_stored = 5;
    ctx.state.configuration_uncommitted_index = 0;
    ctx.log.append_command(2, raft::buffer_type());
    ctx.log.append_command(2, raft::buffer_type());

    raft::entry::list entries = raft_entries::make_entries(1, raft::entry_type::change);
    ASSERT_TRUE(entries_append(ctx, 1, 1, 1, 2, entries));
}

TEST_F(raft_entries, DISABLED_append_resolve_conflicts_uncommitted_index)
{
    details::context& ctx = *m_p_ctx;
    ASSERT_TRUE(ctx.role.is_follower());

    ctx.term = 1;
    ctx.state.commit_index = 2;
    ctx.state.last_stored = 5;
    ctx.state.configuration_uncommitted_index = 5;
    ctx.log.append_command(2, raft::buffer_type());
    ctx.log.append_command(2, raft::buffer_type());

    raft::entry::list entries = raft_entries::make_entries(3, raft::entry_type::change);
    ASSERT_TRUE(entries_append(ctx, 1, 1, 1, 2, entries));
}

TEST_F(raft_entries, append_empty_entries)
{
    details::context& ctx = *m_p_ctx;
    ASSERT_TRUE(ctx.role.is_follower());

    ctx.term = 1;
    ctx.state.commit_index = 1;
    ctx.log.append_change(1, raft::cluster_config());

    raft::entry::list entries;
    ASSERT_TRUE(entries_append(ctx, 1, 1, 1, 1, entries));
}

TEST_F(raft_entries, append_duplicates)
{
    details::context& ctx = *m_p_ctx;
    ASSERT_TRUE(ctx.role.is_follower());

    ctx.term = 1;
    ctx.state.commit_index = 1;
    ctx.log.append_change(1, raft::cluster_config());
    ctx.log.append_change(1, raft::cluster_config());

    raft::entry::list entries = raft_entries::make_entries(1, raft::entry_type::change);;
    ASSERT_TRUE(entries_append(ctx, 1, 1, 1, 1, entries));
}

TEST_F(raft_entries, append_failed_local_term_check)
{
    details::context& ctx = *m_p_ctx;
    ASSERT_TRUE(ctx.role.is_follower());

    ctx.term = 1;
    ctx.state.commit_index = 1;
    ctx.log.append_command(7, raft::buffer_type());

    raft::entry::list entries = raft_entries::make_entries(1, raft::entry_type::change);
    ASSERT_FALSE(entries_append(ctx, 1, 1, 1, 1, entries));
}

TEST_F(raft_entries, append_failed_term_check)
{
    details::context& ctx = *m_p_ctx;
    ASSERT_TRUE(ctx.role.is_follower());;

    raft::entry::list entries = raft_entries::make_entries(1, raft::entry_type::change);
    ASSERT_FALSE(entries_append(ctx, 1, 1, 0, 1, entries));
}

TEST_F(raft_entries, append_failed_io_append)
{
    details::context& ctx = *m_p_ctx;
    ASSERT_TRUE(ctx.role.is_follower());
    m_p_io->is_append = false;

    raft::entry::list entries = raft_entries::make_entries(1, raft::entry_type::change);
    ASSERT_FALSE(entries_append(ctx, 1, 1, 0, 1, entries));
}

TEST_F(raft_entries, append_failed_update_configuration_1)
{
    details::context& ctx = *m_p_ctx;
    ASSERT_TRUE(ctx.role.is_follower());

    raft::entry::list entries = raft_entries::make_entries(1, raft::entry_type::change);
    ASSERT_FALSE(entries_append(ctx, 1, 1, 0, 1, entries));
}

TEST_F(raft_entries, append_failed_update_configuration_2)
{
    details::context& ctx = *m_p_ctx;
    ASSERT_TRUE(ctx.role.is_follower());

    ctx.term = 1;
    ctx.state.commit_index = 1;
    ctx.log.append_command(0, raft::buffer_type());
    ctx.log.append_change(0, raft::cluster_config());

    raft::entry::list entries = raft_entries::make_entries(1, raft::entry_type::change);
    ASSERT_FALSE(entries_append(ctx, 1, 1, 1, 2, entries));
}

TEST_F(raft_entries, append_failed_consistent_log)
{
    details::context& ctx = *m_p_ctx;
    ASSERT_TRUE(ctx.role.is_follower());

    ctx.log.append_change(0, raft::cluster_config());
    raft::entry::list entries = raft_entries::make_entries(1, raft::entry_type::change);
    ASSERT_FALSE(entries_append(ctx, 1, 1, 3, 1, entries));
}

TEST_F(raft_entries, append_failed_resolve_conflicts)
{
    details::context& ctx = *m_p_ctx;
    ASSERT_TRUE(ctx.role.is_follower());
    m_p_io->is_truncate = false;

    ctx.term = 1;
    ctx.state.commit_index = 2;
    ctx.state.configuration_uncommitted_index = 0;
    ctx.log.append_command(2, raft::buffer_type());
    ctx.log.append_command(2, raft::buffer_type());

    raft::entry::list entries = raft_entries::make_entries(1, raft::entry_type::change);
    ASSERT_FALSE(entries_append(ctx, 1, 1, 1, 2, entries));
}

TEST_F(raft_entries, apply_command)
{
    details::context& ctx = *m_p_ctx;
    ASSERT_TRUE(ctx.role.is_follower());
    details::role::become_candidate(ctx);
    ASSERT_TRUE(ctx.role.is_leader());

    ctx.term = 1;
    ASSERT_TRUE(entries_apply_command(ctx, raft::buffer_type(1, 1)));
}

TEST_F(raft_entries, apply_command_failed_empty_buffer)
{
    details::context& ctx = *m_p_ctx;
    ASSERT_TRUE(ctx.role.is_follower());
    details::role::become_candidate(ctx);
    ASSERT_TRUE(ctx.role.is_leader());

    ctx.term = 1;
    ASSERT_FALSE(entries_apply_command(ctx, raft::buffer_type()));
}

TEST_F(raft_entries, apply_command_failed_non_leader)
{
    details::context& ctx = *m_p_ctx;
    ASSERT_TRUE(ctx.role.is_follower());

    ctx.term = 1;
    ASSERT_FALSE(entries_apply_command(ctx, raft::buffer_type(1, 1)));
}

TEST_F(raft_entries, apply_configuration)
{
    details::context& ctx = *m_p_ctx;
    ASSERT_TRUE(ctx.role.is_follower());
    details::role::become_candidate(ctx);
    ASSERT_TRUE(ctx.role.is_leader());

    ctx.term = 1;
    ASSERT_TRUE(entries_apply_configuration(ctx, raft::cluster_config()));
}

TEST_F(raft_entries, apply_configuration_failed_io_append)
{
    details::context& ctx = *m_p_ctx;
    ASSERT_TRUE(ctx.role.is_follower());
    m_p_io->is_append = false;

    ctx.term = 1;
    ASSERT_FALSE(entries_apply_configuration(ctx, raft::cluster_config()));
}

TEST_F(raft_entries, commit)
{
    details::context& ctx = *m_p_ctx;
    ASSERT_TRUE(ctx.role.is_follower());

    ctx.term = 1;
    ASSERT_TRUE(details::replication::entries::commit(ctx));
}

TEST_F(raft_entries, commit_not_existing_entry)
{
    details::context& ctx = *m_p_ctx;
    ASSERT_TRUE(ctx.role.is_follower());

    ctx.term = 1;
    ctx.state.last_applied = 1;
    ctx.state.commit_index = 2;
    ASSERT_TRUE(details::replication::entries::commit(ctx));
}

TEST_F(raft_entries, commit_change)
{
    details::context& ctx = *m_p_ctx;
    ASSERT_TRUE(ctx.role.is_follower());

    ctx.term = 1;
    ctx.state.last_applied = 0;
    ctx.state.commit_index = 1;
    ctx.log.append_change(1, raft::cluster_config());
    ASSERT_TRUE(details::replication::entries::commit(ctx));
}

TEST_F(raft_entries, commit_change_leader)
{
    details::context& ctx = *m_p_ctx;
    ASSERT_TRUE(ctx.role.is_follower());
    details::role::become_candidate(ctx);
    ASSERT_TRUE(ctx.role.is_leader());

    ctx.term = 1;
    ctx.role.is_voter = false;
    ctx.state.last_applied = 0;
    ctx.state.commit_index = 1;
    ctx.log.append_change(1, raft::cluster_config());
    ASSERT_TRUE(details::replication::entries::commit(ctx));
    ASSERT_TRUE(ctx.role.is_follower());
}

TEST_F(raft_entries, commit_change_uncommitted_index)
{
    details::context& ctx = *m_p_ctx;
    ASSERT_TRUE(ctx.role.is_follower());

    ctx.term = 1;
    ctx.state.last_applied = 0;
    ctx.state.commit_index = 1;
    ctx.state.configuration_uncommitted_index = 1;
    ctx.log.append_change(1, raft::cluster_config());
    ASSERT_TRUE(details::replication::entries::commit(ctx));
    ASSERT_TRUE(ctx.state.configuration_uncommitted_index == 0);
}

TEST_F(raft_entries, commit_command)
{
    details::context& ctx = *m_p_ctx;
    ASSERT_TRUE(ctx.role.is_follower());

    ctx.term = 1;
    ctx.state.last_applied = 0;
    ctx.state.commit_index = 1;
    ctx.log.append_command(1, raft::buffer_type());
    ASSERT_TRUE(details::replication::entries::commit(ctx));
}

TEST_F(raft_entries, commit_invalid_entry)
{
    details::context& ctx = *m_p_ctx;
    ASSERT_TRUE(ctx.role.is_follower());

    raft::entry::ptr p_entry = std::make_shared<raft::entry>();
    p_entry->term = 1;
    p_entry->type = (raft::entry_type)255;

    ctx.term = 1;
    ctx.state.last_applied = 0;
    ctx.state.commit_index = 1;
    ctx.log.append(p_entry);
    ASSERT_FALSE(details::replication::entries::commit(ctx));
}

int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
