/*
 * The MIT License
 *
 * Copyright 2026 Chistyakov Alexander.
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

#include <sstream>

#include <gtest/gtest.h>

#include "raft/details/context.h"
#include "raft/details/logger.h"

#include "stub/empty_io.h"
#include "stub/fsm_stub.h"

namespace {

namespace raft = ::wstux::raft;
namespace details = raft::details;
namespace tests = raft::tests;

struct logging_handler : public raft::logging_handler
{
    using ptr = std::unique_ptr<logging_handler>;

    logging_handler()
        : raft::logging_handler()
    {
        p_this = this;
        can_log_fn = &can_log;
        log_fn = &log;
    }

    static bool can_log(void* p_this, severity_level) { return p_this != nullptr; }

    static void log(void* p_this, const severity_level lvl, const char* msg)
    {
        static_cast<logging_handler*>(p_this)->ss << "[" << lvl << "] " << msg << " ";
    }

    std::stringstream ss;
};

class raft_logger : public ::testing::Test
{
public:
    virtual void SetUp() override
    {
        tests::empty_io::ptr p_io = std::make_shared<tests::empty_io>();

        m_p_ctx = std::make_unique<details::context>(1, p_io, std::make_shared<tests::fsm_stub>(), std::make_unique<logging_handler>(),
            []() -> bool { return false; }, []() -> void {});

        p_io->cluster_cfg.servers.emplace_back(1, std::to_string(1), true);
        details::utils::bootstrap(*m_p_ctx, p_io->cluster_cfg);

        m_p_hdlr = static_cast<logging_handler*>(m_p_ctx->raft_logger.p_hdlr->p_this);
    }

    virtual void TearDown() override {}

    details::context& context() { raft::config cfg; return context(cfg); }

    details::context& context(raft::config& cfg)
    {
        cfg.address = "1";
        details::utils::init(*m_p_ctx, cfg);
        return *m_p_ctx;
    }

    const logging_handler& logging_hdlr() const { return *m_p_hdlr; }

private:
    details::context::ptr m_p_ctx;
    logging_handler* m_p_hdlr;
};

} // <anonymous> namespace

TEST_F(raft_logger, root_logging)
{
    details::context& ctx = context();
    const logging_handler& hdlr = logging_hdlr();

    RAFT_LOG_EMERG(ctx,  "LOG_EMERG");
    RAFT_LOG_FATAL(ctx,  "LOG_FATAL");
    RAFT_LOG_CRIT(ctx,   "LOG_CRIT");
    RAFT_LOG_ERROR(ctx,  "LOG_ERROR");
    RAFT_LOG_WARN(ctx,   "LOG_WARN");
    RAFT_LOG_NOTICE(ctx, "LOG_NOTICE");
    RAFT_LOG_INFO(ctx,   "LOG_INFO");
    RAFT_LOG_DEBUG(ctx,  "LOG_DEBUG");
    RAFT_LOG_TRACE(ctx,  "LOG_TRACE");

    const std::string etalon = "[0] LOG_EMERG [1] LOG_FATAL [2] LOG_CRIT [3] LOG_ERROR [4] LOG_WARN "
                               "[5] LOG_NOTICE [6] LOG_INFO [7] LOG_DEBUG [8] LOG_TRACE ";
    const std::string log_msg = hdlr.ss.str();
    ASSERT_TRUE(etalon == log_msg) << etalon << " != " << std::endl << log_msg;
}

TEST_F(raft_logger, append_entries)
{
    details::context& ctx = context();
    const logging_handler& hdlr = logging_hdlr();

    RAFT_AE_LOG_EMERG(ctx,  "LOG_EMERG");
    RAFT_AE_LOG_FATAL(ctx,  "LOG_FATAL");
    RAFT_AE_LOG_CRIT(ctx,   "LOG_CRIT");
    RAFT_AE_LOG_ERROR(ctx,  "LOG_ERROR");
    RAFT_AE_LOG_WARN(ctx,   "LOG_WARN");
    RAFT_AE_LOG_NOTICE(ctx, "LOG_NOTICE");
    RAFT_AE_LOG_INFO(ctx,   "LOG_INFO");
    RAFT_AE_LOG_DEBUG(ctx,  "LOG_DEBUG");
    RAFT_AE_LOG_TRACE(ctx,  "LOG_TRACE");

    const std::string etalon = "[0] <AppendEntries> LOG_EMERG [1] <AppendEntries> LOG_FATAL [2] <AppendEntries> LOG_CRIT "
                               "[3] <AppendEntries> LOG_ERROR [4] <AppendEntries> LOG_WARN [5] <AppendEntries> LOG_NOTICE "
                               "[6] <AppendEntries> LOG_INFO [7] <AppendEntries> LOG_DEBUG [8] <AppendEntries> LOG_TRACE ";
    const std::string log_msg = hdlr.ss.str();
    ASSERT_TRUE(etalon == log_msg) << etalon << " != " << std::endl << log_msg;
}

TEST_F(raft_logger, append_entries_disable)
{
    raft::config cfg;
    cfg.is_append_entries_log_ch_enabled = false;
    details::context& ctx = context(cfg);
    const logging_handler& hdlr = logging_hdlr();

    RAFT_AE_LOG_EMERG(ctx,  "LOG_EMERG");
    RAFT_AE_LOG_FATAL(ctx,  "LOG_FATAL");
    RAFT_AE_LOG_CRIT(ctx,   "LOG_CRIT");
    RAFT_AE_LOG_ERROR(ctx,  "LOG_ERROR");
    RAFT_AE_LOG_WARN(ctx,   "LOG_WARN");
    RAFT_AE_LOG_NOTICE(ctx, "LOG_NOTICE");
    RAFT_AE_LOG_INFO(ctx,   "LOG_INFO");
    RAFT_AE_LOG_DEBUG(ctx,  "LOG_DEBUG");
    RAFT_AE_LOG_TRACE(ctx,  "LOG_TRACE");

    const std::string etalon = "";
    const std::string log_msg = hdlr.ss.str();
    ASSERT_TRUE(etalon == log_msg) << etalon << " != " << std::endl << log_msg;
}

TEST_F(raft_logger, join)
{
    details::context& ctx = context();
    const logging_handler& hdlr = logging_hdlr();

    RAFT_JOIN_LOG_EMERG(ctx,  "LOG_EMERG");
    RAFT_JOIN_LOG_FATAL(ctx,  "LOG_FATAL");
    RAFT_JOIN_LOG_CRIT(ctx,   "LOG_CRIT");
    RAFT_JOIN_LOG_ERROR(ctx,  "LOG_ERROR");
    RAFT_JOIN_LOG_WARN(ctx,   "LOG_WARN");
    RAFT_JOIN_LOG_NOTICE(ctx, "LOG_NOTICE");
    RAFT_JOIN_LOG_INFO(ctx,   "LOG_INFO");
    RAFT_JOIN_LOG_DEBUG(ctx,  "LOG_DEBUG");
    RAFT_JOIN_LOG_TRACE(ctx,  "LOG_TRACE");

    const std::string etalon = "[0] <Join> LOG_EMERG [1] <Join> LOG_FATAL [2] <Join> LOG_CRIT "
                               "[3] <Join> LOG_ERROR [4] <Join> LOG_WARN [5] <Join> LOG_NOTICE "
                               "[6] <Join> LOG_INFO [7] <Join> LOG_DEBUG [8] <Join> LOG_TRACE ";
    const std::string log_msg = hdlr.ss.str();
    ASSERT_TRUE(etalon == log_msg) << etalon << " != " << std::endl << log_msg;
}

TEST_F(raft_logger, join_disable)
{
    raft::config cfg;
    cfg.is_join_log_ch_enabled = false;
    details::context& ctx = context(cfg);
    const logging_handler& hdlr = logging_hdlr();

    RAFT_JOIN_LOG_EMERG(ctx,  "LOG_EMERG");
    RAFT_JOIN_LOG_FATAL(ctx,  "LOG_FATAL");
    RAFT_JOIN_LOG_CRIT(ctx,   "LOG_CRIT");
    RAFT_JOIN_LOG_ERROR(ctx,  "LOG_ERROR");
    RAFT_JOIN_LOG_WARN(ctx,   "LOG_WARN");
    RAFT_JOIN_LOG_NOTICE(ctx, "LOG_NOTICE");
    RAFT_JOIN_LOG_INFO(ctx,   "LOG_INFO");
    RAFT_JOIN_LOG_DEBUG(ctx,  "LOG_DEBUG");
    RAFT_JOIN_LOG_TRACE(ctx,  "LOG_TRACE");

    const std::string etalon = "";
    const std::string log_msg = hdlr.ss.str();
    ASSERT_TRUE(etalon == log_msg) << etalon << " != " << std::endl << log_msg;
}

TEST_F(raft_logger, heartbeat)
{
    details::context& ctx = context();
    const logging_handler& hdlr = logging_hdlr();

    RAFT_HB_LOG_EMERG(ctx,  "LOG_EMERG");
    RAFT_HB_LOG_FATAL(ctx,  "LOG_FATAL");
    RAFT_HB_LOG_CRIT(ctx,   "LOG_CRIT");
    RAFT_HB_LOG_ERROR(ctx,  "LOG_ERROR");
    RAFT_HB_LOG_WARN(ctx,   "LOG_WARN");
    RAFT_HB_LOG_NOTICE(ctx, "LOG_NOTICE");
    RAFT_HB_LOG_INFO(ctx,   "LOG_INFO");
    RAFT_HB_LOG_DEBUG(ctx,  "LOG_DEBUG");
    RAFT_HB_LOG_TRACE(ctx,  "LOG_TRACE");

    const std::string etalon = "[0] <Heartbeat> LOG_EMERG [1] <Heartbeat> LOG_FATAL [2] <Heartbeat> LOG_CRIT "
                               "[3] <Heartbeat> LOG_ERROR [4] <Heartbeat> LOG_WARN [5] <Heartbeat> LOG_NOTICE "
                               "[6] <Heartbeat> LOG_INFO [7] <Heartbeat> LOG_DEBUG [8] <Heartbeat> LOG_TRACE ";
    const std::string log_msg = hdlr.ss.str();
    ASSERT_TRUE(etalon == log_msg) << etalon << " != " << std::endl << log_msg;
}

TEST_F(raft_logger, heartbeat_disable)
{
    raft::config cfg;
    cfg.is_heartbeat_log_ch_enabled = false;
    details::context& ctx = context(cfg);
    const logging_handler& hdlr = logging_hdlr();

    RAFT_HB_LOG_EMERG(ctx,  "LOG_EMERG");
    RAFT_HB_LOG_FATAL(ctx,  "LOG_FATAL");
    RAFT_HB_LOG_CRIT(ctx,   "LOG_CRIT");
    RAFT_HB_LOG_ERROR(ctx,  "LOG_ERROR");
    RAFT_HB_LOG_WARN(ctx,   "LOG_WARN");
    RAFT_HB_LOG_NOTICE(ctx, "LOG_NOTICE");
    RAFT_HB_LOG_INFO(ctx,   "LOG_INFO");
    RAFT_HB_LOG_DEBUG(ctx,  "LOG_DEBUG");
    RAFT_HB_LOG_TRACE(ctx,  "LOG_TRACE");

    const std::string etalon = "";
    const std::string log_msg = hdlr.ss.str();
    ASSERT_TRUE(etalon == log_msg) << etalon << " != " << std::endl << log_msg;
}

TEST_F(raft_logger, snapshot)
{
    details::context& ctx = context();
    const logging_handler& hdlr = logging_hdlr();

    RAFT_SH_LOG_EMERG(ctx,  "LOG_EMERG");
    RAFT_SH_LOG_FATAL(ctx,  "LOG_FATAL");
    RAFT_SH_LOG_CRIT(ctx,   "LOG_CRIT");
    RAFT_SH_LOG_ERROR(ctx,  "LOG_ERROR");
    RAFT_SH_LOG_WARN(ctx,   "LOG_WARN");
    RAFT_SH_LOG_NOTICE(ctx, "LOG_NOTICE");
    RAFT_SH_LOG_INFO(ctx,   "LOG_INFO");
    RAFT_SH_LOG_DEBUG(ctx,  "LOG_DEBUG");
    RAFT_SH_LOG_TRACE(ctx,  "LOG_TRACE");

    const std::string etalon = "[0] <Snapshot> LOG_EMERG [1] <Snapshot> LOG_FATAL [2] <Snapshot> LOG_CRIT "
                               "[3] <Snapshot> LOG_ERROR [4] <Snapshot> LOG_WARN [5] <Snapshot> LOG_NOTICE "
                               "[6] <Snapshot> LOG_INFO [7] <Snapshot> LOG_DEBUG [8] <Snapshot> LOG_TRACE ";
    const std::string log_msg = hdlr.ss.str();
    ASSERT_TRUE(etalon == log_msg) << etalon << " != " << std::endl << log_msg;
}

TEST_F(raft_logger, snapshot_disable)
{
    raft::config cfg;
    cfg.is_snapshot_log_ch_enabled = false;
    details::context& ctx = context(cfg);
    const logging_handler& hdlr = logging_hdlr();

    RAFT_SH_LOG_EMERG(ctx,  "LOG_EMERG");
    RAFT_SH_LOG_FATAL(ctx,  "LOG_FATAL");
    RAFT_SH_LOG_CRIT(ctx,   "LOG_CRIT");
    RAFT_SH_LOG_ERROR(ctx,  "LOG_ERROR");
    RAFT_SH_LOG_WARN(ctx,   "LOG_WARN");
    RAFT_SH_LOG_NOTICE(ctx, "LOG_NOTICE");
    RAFT_SH_LOG_INFO(ctx,   "LOG_INFO");
    RAFT_SH_LOG_DEBUG(ctx,  "LOG_DEBUG");
    RAFT_SH_LOG_TRACE(ctx,  "LOG_TRACE");

    const std::string etalon = "";
    const std::string log_msg = hdlr.ss.str();
    ASSERT_TRUE(etalon == log_msg) << etalon << " != " << std::endl << log_msg;
}

TEST_F(raft_logger, timeout)
{
    details::context& ctx = context();
    const logging_handler& hdlr = logging_hdlr();

    RAFT_TO_LOG_EMERG(ctx,  "LOG_EMERG");
    RAFT_TO_LOG_FATAL(ctx,  "LOG_FATAL");
    RAFT_TO_LOG_CRIT(ctx,   "LOG_CRIT");
    RAFT_TO_LOG_ERROR(ctx,  "LOG_ERROR");
    RAFT_TO_LOG_WARN(ctx,   "LOG_WARN");
    RAFT_TO_LOG_NOTICE(ctx, "LOG_NOTICE");
    RAFT_TO_LOG_INFO(ctx,   "LOG_INFO");
    RAFT_TO_LOG_DEBUG(ctx,  "LOG_DEBUG");
    RAFT_TO_LOG_TRACE(ctx,  "LOG_TRACE");

    const std::string etalon = "[0] <Timeout> LOG_EMERG [1] <Timeout> LOG_FATAL [2] <Timeout> LOG_CRIT "
                               "[3] <Timeout> LOG_ERROR [4] <Timeout> LOG_WARN [5] <Timeout> LOG_NOTICE "
                               "[6] <Timeout> LOG_INFO [7] <Timeout> LOG_DEBUG [8] <Timeout> LOG_TRACE ";
    const std::string log_msg = hdlr.ss.str();
    ASSERT_TRUE(etalon == log_msg) << etalon << " != " << std::endl << log_msg;
}

TEST_F(raft_logger, timeout_disable)
{
    raft::config cfg;
    cfg.is_timeout_log_ch_enabled = false;
    details::context& ctx = context(cfg);
    const logging_handler& hdlr = logging_hdlr();

    RAFT_TO_LOG_EMERG(ctx,  "LOG_EMERG");
    RAFT_TO_LOG_FATAL(ctx,  "LOG_FATAL");
    RAFT_TO_LOG_CRIT(ctx,   "LOG_CRIT");
    RAFT_TO_LOG_ERROR(ctx,  "LOG_ERROR");
    RAFT_TO_LOG_WARN(ctx,   "LOG_WARN");
    RAFT_TO_LOG_NOTICE(ctx, "LOG_NOTICE");
    RAFT_TO_LOG_INFO(ctx,   "LOG_INFO");
    RAFT_TO_LOG_DEBUG(ctx,  "LOG_DEBUG");
    RAFT_TO_LOG_TRACE(ctx,  "LOG_TRACE");

    const std::string etalon = "";
    const std::string log_msg = hdlr.ss.str();
    ASSERT_TRUE(etalon == log_msg) << etalon << " != " << std::endl << log_msg;
}

TEST_F(raft_logger, vote)
{
    details::context& ctx = context();
    const logging_handler& hdlr = logging_hdlr();

    RAFT_VOTE_LOG_EMERG(ctx,  "LOG_EMERG");
    RAFT_VOTE_LOG_FATAL(ctx,  "LOG_FATAL");
    RAFT_VOTE_LOG_CRIT(ctx,   "LOG_CRIT");
    RAFT_VOTE_LOG_ERROR(ctx,  "LOG_ERROR");
    RAFT_VOTE_LOG_WARN(ctx,   "LOG_WARN");
    RAFT_VOTE_LOG_NOTICE(ctx, "LOG_NOTICE");
    RAFT_VOTE_LOG_INFO(ctx,   "LOG_INFO");
    RAFT_VOTE_LOG_DEBUG(ctx,  "LOG_DEBUG");
    RAFT_VOTE_LOG_TRACE(ctx,  "LOG_TRACE");

    const std::string etalon = "[0] <Vote> LOG_EMERG [1] <Vote> LOG_FATAL [2] <Vote> LOG_CRIT "
                               "[3] <Vote> LOG_ERROR [4] <Vote> LOG_WARN [5] <Vote> LOG_NOTICE "
                               "[6] <Vote> LOG_INFO [7] <Vote> LOG_DEBUG [8] <Vote> LOG_TRACE ";
    const std::string log_msg = hdlr.ss.str();
    ASSERT_TRUE(etalon == log_msg) << etalon << " != " << std::endl << log_msg;
}

TEST_F(raft_logger, vote_disable)
{
    raft::config cfg;
    cfg.is_vote_log_ch_enabled = false;
    details::context& ctx = context(cfg);
    const logging_handler& hdlr = logging_hdlr();

    RAFT_VOTE_LOG_EMERG(ctx,  "LOG_EMERG");
    RAFT_VOTE_LOG_FATAL(ctx,  "LOG_FATAL");
    RAFT_VOTE_LOG_CRIT(ctx,   "LOG_CRIT");
    RAFT_VOTE_LOG_ERROR(ctx,  "LOG_ERROR");
    RAFT_VOTE_LOG_WARN(ctx,   "LOG_WARN");
    RAFT_VOTE_LOG_NOTICE(ctx, "LOG_NOTICE");
    RAFT_VOTE_LOG_INFO(ctx,   "LOG_INFO");
    RAFT_VOTE_LOG_DEBUG(ctx,  "LOG_DEBUG");
    RAFT_VOTE_LOG_TRACE(ctx,  "LOG_TRACE");

    const std::string etalon = "";
    const std::string log_msg = hdlr.ss.str();
    ASSERT_TRUE(etalon == log_msg) << etalon << " != " << std::endl << log_msg;
}

int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
