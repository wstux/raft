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

#include "raft/server.h"

#include "stub/empty_io.h"

namespace {

namespace raft = ::wstux::raft;
namespace details = raft::details;
namespace tests = raft::tests;

class raft_server : public ::testing::Test
{
public:
    virtual void SetUp() override
    {
        m_p_io = std::make_shared<tests::empty_io>();
        m_p_io->cluster_cfg.servers.emplace_back(1, true);

        tests::empty_io* p_raw_io = m_p_io.get();
        std::function<bool()> is_stop_fn = [p_raw_io]()->bool { return p_raw_io->is_stop; };

        m_p_srv = std::make_shared<raft::server>(1, m_p_io, raft::logging_handler::ptr(), is_stop_fn);
    }

    virtual void TearDown() override {}

protected:
    tests::empty_io::ptr m_p_io;
    raft::server::ptr m_p_srv;
};

} // <anonymous> namespace

TEST_F(raft_server, failed_init)
{
    m_p_io->is_init = false;

    EXPECT_FALSE(m_p_srv->init());
}

TEST_F(raft_server, double_start)
{
    EXPECT_TRUE(m_p_srv->init());
    EXPECT_TRUE(m_p_srv->start());
    EXPECT_FALSE(m_p_srv->start());
}

TEST_F(raft_server, start_without_init)
{
    EXPECT_FALSE(m_p_srv->start());
}

TEST_F(raft_server, reconfigure)
{
    EXPECT_TRUE(m_p_srv->init());
    EXPECT_TRUE(m_p_srv->start());

    EXPECT_TRUE(m_p_srv->reconfigure());
}

TEST_F(raft_server, reconfigure_invalid_configuration)
{
    EXPECT_TRUE(m_p_srv->init());
    EXPECT_TRUE(m_p_srv->start());

    m_p_io->cfg.heartbeat_interval_ms = 0;
    EXPECT_FALSE(m_p_srv->reconfigure());
}

TEST_F(raft_server, stop_without_start)
{
    EXPECT_NO_THROW(m_p_srv->stop());
}

TEST_F(raft_server, handle_invalid_message)
{
    EXPECT_TRUE(m_p_srv->init());
    EXPECT_TRUE(m_p_srv->start());

    char buff[raft::details::message::size];
    std::fill(buff, buff + raft::details::message::size, 0);
    raft::buffer_type buffer(buff, raft::details::message::size);
    EXPECT_NO_THROW(m_p_srv->handle_message(buffer));
}

int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
