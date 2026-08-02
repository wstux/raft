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

#include "raft_le/details/context.h"

#include "stub/empty_io.h"

namespace {

namespace raft = ::wstux::raft::le;
namespace details = raft::details;
namespace tests = raft::tests;

class raft_context : public ::testing::Test
{
public:
    virtual void SetUp() override {}
    virtual void TearDown() override {}

    static details::context::ptr context(size_t cluster_size = 0, bool is_voter = true)
    {
        tests::return_type rt = tests::make_context(cluster_size, is_voter);
        details::context::ptr p_ctx = std::move(rt.first);
        if (! details::utils::init(*p_ctx, 1)) {
            return p_ctx;
        }
        if (! details::utils::load(*p_ctx)) {
            return p_ctx;
        }
        return p_ctx;
    }
};

} // <anonymous> namespace

TEST_F(raft_context, init)
{
    tests::return_type rt = tests::make_context(1, true);
    details::context::ptr p_ctx = std::move(rt.first);

    EXPECT_TRUE(details::utils::init(*p_ctx, 1));
}

TEST_F(raft_context, init_failed)
{
    tests::return_type rt = tests::make_context(1, true);
    details::context::ptr p_ctx = std::move(rt.first);
    tests::empty_io::ptr p_io = rt.second;

    p_io->is_init = false;
    EXPECT_FALSE(details::utils::init(*p_ctx, 1));
}

TEST_F(raft_context, init_invalid_config)
{
    tests::return_type rt = tests::make_context(1, true);
    details::context::ptr p_ctx = std::move(rt.first);
    tests::empty_io::ptr p_io = rt.second;

    p_io->cfg.heartbeat_interval_ms = 0;
    EXPECT_FALSE(details::utils::init(*p_ctx, 1));
}

TEST_F(raft_context, load)
{
    tests::return_type rt = tests::make_context(1, true);
    details::context::ptr p_ctx = std::move(rt.first);

    EXPECT_TRUE(details::utils::init(*p_ctx, 1));
    EXPECT_TRUE(details::utils::load(*p_ctx));
}

TEST_F(raft_context, load_failed)
{
    tests::return_type rt = tests::make_context(1, true);
    details::context::ptr p_ctx = std::move(rt.first);
    tests::empty_io::ptr p_io = rt.second;

    p_io->is_load = false;
    EXPECT_TRUE(details::utils::init(*p_ctx, 1));
    EXPECT_FALSE(details::utils::load(*p_ctx));
}

TEST_F(raft_context, load_failed_duplicated_peer)
{
    tests::return_type rt = tests::make_context(3, true);
    details::context::ptr p_ctx = std::move(rt.first);
    tests::empty_io::ptr p_io = rt.second;

    p_io->cluster_cfg.servers.emplace_back(2, "2", true);
    EXPECT_TRUE(details::utils::init(*p_ctx, 1));
    EXPECT_FALSE(details::utils::load(*p_ctx));
}

TEST_F(raft_context, to_string)
{
    tests::return_type rt = tests::make_context(3, true);
    details::context::ptr p_ctx = std::move(rt.first);

    p_ctx->role.role = details::role::role_type::candidate;
    std::stringstream ss;
    ss << (*p_ctx);
    EXPECT_TRUE(ss.str() == "1(candidate)");
}

int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
