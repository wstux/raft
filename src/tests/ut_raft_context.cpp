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

#include <sstream>

#include <gtest/gtest.h>

#include "raft/details/context.h"

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
        std::function<bool()> is_stop_fn = [p_raw_io]()->bool { return p_raw_io->is_stop; };

        m_p_ctx = std::make_unique<details::context>(1, m_p_io, m_p_fsm, raft::logging_handler::ptr(), is_stop_fn);
    }

    virtual void TearDown() override {}

    details::context& init(size_t servs_count = 1, bool is_voter = true)
    {
        for (size_t i = 0; i < servs_count; ++i) {
            m_p_io->cluster_cfg.servers.emplace_back(i + 1, (i == 0) ? is_voter : true);
        }

        return *m_p_ctx;
    }

protected:
    tests::empty_io::ptr m_p_io;
    tests::fsm_stub::ptr m_p_fsm;
    details::context::ptr m_p_ctx;
};

} // <anonymous> namespace

TEST_F(raft_context, init)
{
    details::context& ctx = init(1);

    EXPECT_TRUE(details::utils::init(ctx));
}

TEST_F(raft_context, init_failed)
{
    details::context& ctx = init(1);

    m_p_io->is_init = false;
    EXPECT_FALSE(details::utils::init(ctx));
}

TEST_F(raft_context, init_invalid_config)
{
    details::context& ctx = init(1);

    m_p_io->cfg.heartbeat_interval_ms = 0;
    EXPECT_FALSE(details::utils::init(ctx));
}

TEST_F(raft_context, load)
{
    details::context& ctx = init(1);

    EXPECT_TRUE(details::utils::init(ctx));
    EXPECT_TRUE(details::utils::load(ctx));
}

TEST_F(raft_context, load_failed)
{
    details::context& ctx = init(1);
    EXPECT_TRUE(details::utils::init(ctx));

    ctx.peers.emplace_back(raft::server_config(2, true));
    EXPECT_FALSE(details::utils::load(ctx));
}

TEST_F(raft_context, load_failed_duplicated_peer)
{
    details::context& ctx = init(3);

    m_p_io->cluster_cfg.servers.emplace_back(2, true);
    EXPECT_TRUE(details::utils::init(ctx));
    EXPECT_FALSE(details::utils::load(ctx));
}

TEST_F(raft_context, to_string)
{
    details::context& ctx = init(3);

    ctx.role.role = details::role::role_type::candidate;
    std::stringstream ss;
    ss << ctx;
    EXPECT_TRUE(ss.str() == "1(candidate)");
}

int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
