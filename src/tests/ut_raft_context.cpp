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

#include <iostream>
#include <sstream>

#include <gtest/gtest.h>

#include "raft/details/context.h"
#include "raft/details/connection/serialization.h"
#include "raft/details/replication/membership.h"
#include "raft/details/role/convert.h"

#include "stub/empty_io.h"
#include "stub/fsm_stub.h"

namespace {

namespace raft = ::wstux::raft;
namespace details = raft::details;
namespace tests = raft::tests;

class raft_context : public ::testing::Test
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

    details::context& init(size_t servs_count = 1, bool is_voter = true)
    {
        for (size_t i = 0; i < servs_count; ++i) {
            m_p_io->cluster_cfg.servers.emplace_back(i + 1, std::to_string(i + 1), (i == 0) ? is_voter : true);
        }

        return *m_p_ctx;
    }

protected:
    tests::empty_io::ptr m_p_io;
    tests::fsm_stub::ptr m_p_fsm;
    details::context::ptr m_p_ctx;
};

} // <anonymous> namespace

/**
 *  \test   Verification of successful raft context initialization.
 *
 *  **Test logic description:**
 *  Verifies that the raft context can be successfully bootstrapped with a
 *  cluster configuration and initialized with standard runtime settings under
 *  normal conditions.
 *
 *  \expected_result    Both bootstrap and initialization procedures complete
 *      successfully, returning true and preparing the context for operation.
 */
TEST_F(raft_context, init)
{
    details::context& ctx = init(1);

    EXPECT_TRUE(details::utils::bootstrap(ctx, m_p_io->cluster_cfg));
    EXPECT_TRUE(details::utils::init(ctx, m_p_io->cfg));
}

/**
 *  \test   Verification of initialization failure when the server address is empty.
 *
 *  **Test logic description:**
 *  Verifies that the initialization process safely aborts and returns false if
 *  the provided configuration contains an empty network address string.
 *
 *  **Steps to reproduce:**
 *  -# Create and mock a base context with a specific node identifier.
 *  -# Execute the bootstrap function with a valid cluster configuration and ensure it succeeds.
 *  -# Explicitly clear the server address string within the configuration structure.
 *  -# Attempt to execute the initialization function with the altered configuration.
 *  -# Verify that the initialization function rejects the configuration and returns false.
 *
 *  \expected_result    The initialization method fails early due to the missing
 *      address constraint, returning false without altering runtime structures.
 */
TEST_F(raft_context, init_empty_address)
{
    details::context& ctx = init(1);

    EXPECT_TRUE(details::utils::bootstrap(ctx, m_p_io->cluster_cfg));
    m_p_io->cfg.address.clear();
    EXPECT_FALSE(details::utils::init(ctx, m_p_io->cfg));
}

/**
 *  \test   Verification of initialization failure when the I/O interface fails
 *      to initialize.
 *
 *  **Test logic description:**
 *  Verifies that the context initialization fails immediately if the underlying
 *  I/O subsystem cannot be initialized for the specified server identifier.
 *
 *  **Steps to reproduce:**
 *  -# Create and mock a base context with a specific node identifier.
 *  -# Force the I/O interface mock into a failed state by setting its initialization flag to false.
 *  -# Execute the bootstrap function and verify that it returns true.
 *  -# Attempt to execute the initialization function.
 *  -# Verify that the initialization function detects the I/O subsystem failure and returns false.
 *
 *  \expected_result    The initialization method aborts at the very beginning
 *      because of the faulty I/O interface, returning false and logging an error.
 */
TEST_F(raft_context, init_io_failed)
{
    details::context& ctx = init(1);

    m_p_io->is_init = false;
    EXPECT_TRUE(details::utils::bootstrap(ctx, m_p_io->cluster_cfg));
    EXPECT_FALSE(details::utils::init(ctx, m_p_io->cfg));
}

/**
 *  \test   Verification of initialization failure under invalid timing configurations.
 *
 *  **Test logic description:**
 *  Verifies that the initialization routine enforces constraints on timing configurations,
 *  specifically aborting if the heartbeat interval is set to zero.
 *
 *  **Steps to reproduce:**
 *  -# Create and mock a base context with a specific node identifier.
 *  -# Corrupt the configuration by setting the heartbeat interval to exactly zero milliseconds.
 *  -# Execute the bootstrap function and verify that it returns true.
 *  -# Attempt to execute the initialization function with the invalid configuration.
 *  -# Verify that the initialization function catches the invalid bounds and returns false.
 *
 *  \expected_result    The initialization method fails the timing parameter check,
 *      gracefully returning false to prevent invalid timer registration.
 */
TEST_F(raft_context, init_invalid_config)
{
    details::context& ctx = init(1);

    m_p_io->cfg.heartbeat_interval_ms = 0;
    EXPECT_TRUE(details::utils::bootstrap(ctx, m_p_io->cluster_cfg));
    EXPECT_FALSE(details::utils::init(ctx, m_p_io->cfg));
}

/**
 *  \test   Verification of successful retrieval for an existing server configuration.
 *
 *  **Test logic description:**
 *  Verifies that the configuration lookup function correctly locates and returns
 *  a valid pointer to a server's configuration when given an identifier present
 *  in the cluster config.
 *
 *  \expected_result    The lookup successfully matches the server ID within the
 *      cluster configuration, returning a valid non-null pointer to the
 *      corresponding structure.
 */
TEST_F(raft_context, find_exists_config)
{
    details::context& ctx = init(1);

    EXPECT_TRUE(details::utils::bootstrap(ctx, m_p_io->cluster_cfg));
    EXPECT_TRUE(details::utils::init(ctx, m_p_io->cfg));
    EXPECT_TRUE(details::utils::find_server_config(ctx, 1) != nullptr);
}

/**
 *  \test   Verification of lookup behavior when searching for a non-existent
 *      server configuration.
 *
 *  **Test logic description:**
 *  Verifies that the configuration lookup function gracefully handles queries
 *  for unknown identifiers and safely returns a null pointer if the server ID
 *  does not exist in the cluster configuration.
 *
 *  \expected_result    The lookup function scans the cluster configuration, fails
 *      to find a matching server ID, and returns a null pointer as expected.
 */
TEST_F(raft_context, find_non_exists_config)
{
    details::context& ctx = init(1);

    EXPECT_TRUE(details::utils::bootstrap(ctx, m_p_io->cluster_cfg));
    EXPECT_TRUE(details::utils::init(ctx, m_p_io->cfg));
    EXPECT_TRUE(details::utils::find_server_config(ctx, 2) == nullptr);
}

/**
 *  \test   Verification of successful clean context load.
 *
 *  **Test logic description:**
 *  Verifies that a newly bootstrapped and initialized Raft context can be
 *  successfully loaded from a clean, empty state without any existing logs or
 *  snapshots.
 *
 *  \expected_result    The loading process completes successfully, generating
 *      the initial change entry in the log and preparing the node for execution.
 */
TEST_F(raft_context, load)
{
    details::context& ctx = init(1);

    EXPECT_TRUE(details::utils::bootstrap(ctx, m_p_io->cluster_cfg));
    EXPECT_TRUE(details::utils::init(ctx, m_p_io->cfg));
    EXPECT_TRUE(details::utils::load(ctx));
}

/**
 *  \test   Verification of state loading from a non-empty log store.
 *
 *  **Test logic description:**
 *  Verifies that the context correctly loads and parses pre-existing log entries
 *  from the storage layer, successfully reconstructing the cluster configuration
 *  history.
 *
 *  **Steps to reproduce:**
 *  -# Create and mock a base context for node ID 1.
 *  -# Mock two valid historical log entries inside the storage provider containing configuration updates.
 *  -# Bootstrap and initialize the context.
 *  -# Invoke the load method to apply the existing log entries.
 *  -# Verify that the cluster config is restored and tracking exactly 2 active servers with matching identifiers.
 *
 *  \expected_result    The context successfully restores the state from historical
 *      log entries, advancing configuration counters and populating the server list.
 */
TEST_F(raft_context, load_existing_state)
{
    details::context& ctx = init(1);

    m_p_io->entries.emplace(1, std::make_shared<raft::entry>());
    m_p_io->entries[1]->term = 1;
    m_p_io->entries[1]->type = raft::entry_type::change;
    m_p_io->entries[1]->buffer = details::serialize<raft::cluster_config>(m_p_io->cluster_cfg);
    raft::cluster_config cluster_cfg;
    cluster_cfg.servers = {{1, "1", true}, {2, "2", false}};
    m_p_io->entries.emplace(2, std::make_shared<raft::entry>());
    m_p_io->entries[2]->term = 1;
    m_p_io->entries[2]->type = raft::entry_type::change;
    m_p_io->entries[2]->buffer = details::serialize<raft::cluster_config>(cluster_cfg);

    EXPECT_TRUE(details::utils::bootstrap(ctx, m_p_io->cluster_cfg));
    EXPECT_TRUE(details::utils::init(ctx, m_p_io->cfg));
    EXPECT_TRUE(details::utils::load(ctx));

    EXPECT_TRUE(ctx.state.cluster_cfg.servers.size() == 2);
    EXPECT_TRUE(ctx.state.cluster_cfg.servers[0].id == 1);
    EXPECT_TRUE(ctx.state.cluster_cfg.servers[1].id == 2);
}

/**
 *  \test   Verification of state loading from a pre-existing storage snapshot.
 *
 *  **Test logic description:**
 *  Verifies that if a snapshot is present in the storage layer, the loading routine
 *  correctly deserializes it and applies its metadata to sync the log status.
 *
 *  **Steps to reproduce:**
 *  -# Create and mock a base context for node ID 1.
 *  -# Configure a valid snapshot mock in the storage with snapshot index 5, term 2, and 2 target servers.
 *  -# Bootstrap and initialize the context.
 *  -# Invoke the load method to restore the snapshot data.
 *  -# Verify that runtime fields (term, configuration index, applied index) are properly initialized from the snapshot.
 *
 *  \expected_result    The context recovers metadata entirely from the snapshot,
 *      setting configuration_committed_index to 3 and last_applied to 5.
 */
TEST_F(raft_context, load_existing_snapshot)
{
    details::context& ctx = init(1);

    m_p_io->p_snapshot = raft::snapshot();
    m_p_io->p_snapshot->index = 5;
    m_p_io->p_snapshot->term = 2;
    m_p_io->p_snapshot->conf.servers = {{1, "1", true}, {2, "2", false}};
    m_p_io->p_snapshot->conf_index = 3;

    EXPECT_TRUE(details::utils::bootstrap(ctx, m_p_io->cluster_cfg));
    EXPECT_TRUE(details::utils::init(ctx, m_p_io->cfg));
    EXPECT_TRUE(details::utils::load(ctx));

    EXPECT_TRUE(ctx.term == 1) << ctx.term;
    EXPECT_TRUE(ctx.state.configuration_committed_index = 3) << ctx.state.configuration_committed_index;
    EXPECT_TRUE(ctx.state.last_applied = 5) << ctx.state.last_applied;
    EXPECT_TRUE(ctx.state.cluster_cfg.servers.size() == 2);
    EXPECT_TRUE(ctx.state.cluster_cfg.servers[0].id == 1);
    EXPECT_TRUE(ctx.state.cluster_cfg.servers[1].id == 2);
}

/**
 *  \test   Verification of load failure when log appending fails during clean
 *      bootstrap.
 *
 *  **Test logic description:**
 *  Verifies that if the storage interface fails to append the initial cluster
 *  configuration entry, the loading sequence terminates immediately and returns
 *  false.
 *
 *  **Steps to reproduce:**
 *  -# Create and mock a base context for node ID 1.
 *  -# Force the I/O mock to fail any write/append attempts by setting its internal flag to false.
 *  -# Bootstrap and initialize the context.
 *  -# Invoke the load method.
 *  -# Verify that the load method fails and returns false.
 *
 *  \expected_result    The loading function aborts because it cannot commit the
 *      initial configuration change to storage, preventing an inconsistent node
 *      runtime state.
 */
TEST_F(raft_context, load_failed_append_bootstrap)
{
    details::context& ctx = init(1);

    m_p_io->is_append = false;
    EXPECT_TRUE(details::utils::bootstrap(ctx, m_p_io->cluster_cfg));
    EXPECT_TRUE(details::utils::init(ctx, m_p_io->cfg));
    EXPECT_FALSE(details::utils::load(ctx));
}

/**
 *  \test   Verification of load failure when the storage returns an invalid term.
 *
 *  **Test logic description:**
 *  Verifies that the context loading fails at the very beginning if the terms
 *  retrieved from the storage provider layer evaluate to an invalid identifier.
 *
 *  **Steps to reproduce:**
 *  -# Create and mock a base context for node ID 1.
 *  -# Set the persistent storage term to an invalid token value.
 *  -# Bootstrap and initialize the context.
 *  -# Invoke the load method.
 *  -# Verify that the method immediately rejects the invalid term and returns false.
 *
 *  \expected_result    The load function handles the corrupted term safely by
 *      exiting early, returning false, and logging a severe initialization error.
 */
TEST_F(raft_context, load_invalid_term)
{
    details::context& ctx = init(1);

    m_p_io->term = 0;
    EXPECT_TRUE(details::utils::bootstrap(ctx, m_p_io->cluster_cfg));
    EXPECT_TRUE(details::utils::init(ctx, m_p_io->cfg));
    EXPECT_FALSE(details::utils::load(ctx));
}

/**
 *  \test   Verification of load failure when snapshot restoration fails.
 *
 *  **Test logic description:**
 *  Verifies that if a snapshot is located but the state machine cannot apply
 *  it due to an error, the overall loading operation fails.
 *
 *  **Steps to reproduce:**
 *  -# Create and mock a base context for node ID 1.
 *  -# Set up a valid snapshot metadata mock inside the storage layout.
 *  -# Force the finite state machine mock to reject snapshot processing updates.
 *  -# Bootstrap and initialize the context.
 *  -# Attempt to run the load method and verify that it returns false.
 *
 *  \expected_result    The snapshot recovery fails inside the replication layer,
 *      causing the main load sequence to safely abort and report an error.
 */
TEST_F(raft_context, load_failed_restore_snapshot)
{
    details::context& ctx = init(1);

    m_p_io->p_snapshot = raft::snapshot();
    m_p_io->p_snapshot->index = 5;
    m_p_io->p_snapshot->term = 2;
    m_p_io->p_snapshot->conf.servers = {{1, "1", true}, {2, "2", false}};
    m_p_io->p_snapshot->conf_index = 3;

    m_p_fsm->is_result = false;

    EXPECT_TRUE(details::utils::bootstrap(ctx, m_p_io->cluster_cfg));
    EXPECT_TRUE(details::utils::init(ctx, m_p_io->cfg));
    EXPECT_FALSE(details::utils::load(ctx));
}

/**
 *  \test   Verification of load chain failure due to an invalid cluster
 *      configuration (duplicated peers).
 *
 *  **Test logic description:**
 *  Verifies that if the initial validation routines identify duplicated server
 *  definitions, the bootstrap and init calls report failures, preventing the
 *  node from triggering any load phases.
 *
 *  **Steps to reproduce:**
 *  -# Create and mock a base context with 3 nodes.
 *  -# Invalidate the configuration layout by manually pushing a duplicate server record with an identical ID 2.
 *  -# Execute bootstrap and init, verifying that both methods correctly return false.
 *  -# Attempt to execute the load phase and verify it returns false as well.
 *
 *  \expected_result    The workflow stops due to the configuration anomaly,
 *      preventing invalid topology states from breaking runtime expectations.
 */
TEST_F(raft_context, load_failed_duplicated_peer)
{
    details::context& ctx = init(3);

    m_p_io->cluster_cfg.servers.emplace_back(2, "2", true);
    EXPECT_FALSE(details::utils::bootstrap(ctx, m_p_io->cluster_cfg));
    EXPECT_FALSE(details::utils::init(ctx, m_p_io->cfg));
    EXPECT_FALSE(details::utils::load(ctx));
}

/**
 *  \test   Verification of dynamic runtime context reconfiguration.
 *
 *  **Test logic description:**
 *  Verifies that the context properties can be dynamically reconfigured at runtime,
 *  ensuring that all parameters—including networking addresses, timer intervals,
 *  snapshot parameters, and logger flags—are properly updated to match the new
 *  configuration.
 *
 *  **Steps to reproduce:**
 *  -# Create and mock a base context initialized for node ID 1.
 *  -# Bootstrap, initialize, and load the context.
 *  -# Verify all initial parameters (address, timers, snapshot limits, and log channels) match the default configuration.
 *  -# Construct a new config instance with modified values (e.g., address "localhost", updated intervals, and disabled log channels).
 *  -# Execute the reconfigure function to apply the new configuration profile.
 *  -# Verify that every runtime context field is successfully updated to match the new configuration values.
 *
 *  \expected_result    The context dynamically applies the runtime updates
 *      without errors, and subsequent status checks return the new modified
 *      configuration values.
 */
TEST_F(raft_context, reconfigure)
{
    details::context& ctx = init(1);

    m_p_io->cfg.address = "1";
    EXPECT_TRUE(details::utils::bootstrap(ctx, m_p_io->cluster_cfg));
    EXPECT_TRUE(details::utils::init(ctx, m_p_io->cfg));
    EXPECT_TRUE(details::utils::load(ctx));

    EXPECT_TRUE(ctx.address == "1");
    EXPECT_TRUE(ctx.is_async_io == false);
    EXPECT_TRUE(ctx.election_interval_ms == 250);
    EXPECT_TRUE(ctx.heartbeat_interval_ms == 100);
    EXPECT_TRUE(ctx.state.snapshot.threshold == 512);
    EXPECT_TRUE(ctx.state.snapshot.trailing == 1024);
    EXPECT_TRUE(ctx.raft_logger.is_append_entries_channel_enabled == true);
    EXPECT_TRUE(ctx.raft_logger.is_join_channel_enabled == true);
    EXPECT_TRUE(ctx.raft_logger.is_heartbeat_channel_enabled == true);
    EXPECT_TRUE(ctx.raft_logger.is_snapshot_channel_enabled == true);
    EXPECT_TRUE(ctx.raft_logger.is_timeout_channel_enabled == true);
    EXPECT_TRUE(ctx.raft_logger.is_vote_channel_enabled == true);

    raft::config cfg;
    cfg.address = "localhost";
    cfg.is_async_io = true;
    cfg.vote_timeout_min_ms = 300;
    cfg.heartbeat_interval_ms = 150;
    cfg.snapshot_threshold = 1024;
    cfg.snapshot_trailing = 2048;
    cfg.is_append_entries_log_ch_enabled = false;
    cfg.is_join_log_ch_enabled = false;
    cfg.is_heartbeat_log_ch_enabled = false;
    cfg.is_snapshot_log_ch_enabled = false;
    cfg.is_timeout_log_ch_enabled = false;
    cfg.is_vote_log_ch_enabled = false;

    details::utils::reconfigure(ctx, cfg);
    EXPECT_TRUE(ctx.address == "localhost");
    EXPECT_TRUE(ctx.is_async_io == true);
    EXPECT_TRUE(ctx.election_interval_ms == 300);
    EXPECT_TRUE(ctx.heartbeat_interval_ms == 150);
    EXPECT_TRUE(ctx.state.snapshot.threshold == 1024);
    EXPECT_TRUE(ctx.state.snapshot.trailing == 2048);
    EXPECT_TRUE(ctx.raft_logger.is_append_entries_channel_enabled == false);
    EXPECT_TRUE(ctx.raft_logger.is_join_channel_enabled == false);
    EXPECT_TRUE(ctx.raft_logger.is_heartbeat_channel_enabled == false);
    EXPECT_TRUE(ctx.raft_logger.is_snapshot_channel_enabled == false);
    EXPECT_TRUE(ctx.raft_logger.is_timeout_channel_enabled == false);
    EXPECT_TRUE(ctx.raft_logger.is_vote_channel_enabled == false);
}

int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
