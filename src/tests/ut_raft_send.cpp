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

#include <thread>

#include <gtest/gtest.h>

#include "raft/details/connection/send.h"

#include "stub/empty_io.h"
#include "stub/fsm_stub.h"

namespace {

namespace raft = ::wstux::raft;
namespace details = raft::details;
namespace tests = raft::tests;

class raft_send : public ::testing::Test
{
public:
    virtual void SetUp() override
    {
        m_p_io = std::make_shared<tests::empty_io>();
        m_p_fsm = std::make_shared<tests::fsm_stub>();

        tests::empty_io* p_raw_io = m_p_io.get();
        std::function<bool()> is_stop_fn = [p_raw_io]()->bool { return p_raw_io->is_stop; };

        m_p_ctx = std::make_unique<details::context>(1, m_p_io, m_p_fsm, raft::logging_handler::ptr(), is_stop_fn);

        m_p_io->cluster_cfg.servers.emplace_back(1, "1", true);
        details::utils::bootstrap(*m_p_ctx, m_p_io->cluster_cfg);
        details::utils::init(*m_p_ctx, m_p_io->cfg);
        details::utils::load(*m_p_ctx);
        m_p_ctx->schd.start();
    }

    virtual void TearDown() override {}

    tests::empty_client::ptr client(raft::server_id_t id) const
    {
        std::unordered_map<raft::server_id_t, tests::empty_client::ptr>::const_iterator it = m_p_io->clients.find(id);
        if (it == m_p_io->clients.cend()) {
            return nullptr;
        }
        return it->second;
    }

    void wait(const raft::server_id_t id, const size_t limit_ms = 1500)
    {
        using namespace std::chrono_literals;

        std::unordered_map<raft::server_id_t, tests::empty_client::ptr>::const_iterator it = m_p_io->clients.find(id);
        for (size_t i = 0; (i < limit_ms) && (it == m_p_io->clients.cend()); i += 10) {
            std::this_thread::sleep_for(10ms);
            it = m_p_io->clients.find(id);
        }
    }

protected:
    tests::empty_io::ptr m_p_io;
    tests::fsm_stub::ptr m_p_fsm;
    details::context::ptr m_p_ctx;
};

} // <anonymous> namespace

TEST_F(raft_send, append_entries_request)
{
    details::context& ctx = *m_p_ctx;

    const raft::server_id_t dst_id = 2;
    tests::empty_client::ptr p_client = client(dst_id);
    EXPECT_FALSE(p_client);

    details::utils::send_append_entries_request(ctx, dst_id, "2", 1, 13, 13, 13, raft::entry::list());
    wait(dst_id);
    p_client = client(dst_id);
    ASSERT_TRUE(p_client);
    EXPECT_TRUE(p_client->address == "2") << p_client->address;
}

TEST_F(raft_send, append_entries_response)
{
    details::context& ctx = *m_p_ctx;

    const raft::server_id_t dst_id = 2;
    tests::empty_client::ptr p_client = client(dst_id);
    EXPECT_FALSE(p_client);

    details::utils::send_append_entries_response(ctx, dst_id, "2", 1, false, 13);
    wait(dst_id);
    p_client = client(dst_id);
    ASSERT_TRUE(p_client);
    EXPECT_TRUE(p_client->address == "2") << p_client->address;
}

TEST_F(raft_send, send_join_request)
{
    details::context& ctx = *m_p_ctx;

    const raft::server_id_t dst_id = 2;
    tests::empty_client::ptr p_client = client(dst_id);
    EXPECT_FALSE(p_client);

    details::utils::send_join_request(ctx, dst_id, "2", 1, 2, "2", true);
    wait(dst_id);
    p_client = client(dst_id);
    ASSERT_TRUE(p_client);
    EXPECT_TRUE(p_client->address == "2") << p_client->address;
}

TEST_F(raft_send, send_join_response)
{
    details::context& ctx = *m_p_ctx;

    const raft::server_id_t dst_id = 2;
    tests::empty_client::ptr p_client = client(dst_id);
    EXPECT_FALSE(p_client);

    details::utils::send_join_response(ctx, dst_id, "2", 1, 0);
    wait(dst_id);
    p_client = client(dst_id);
    ASSERT_TRUE(p_client);
    EXPECT_TRUE(p_client->address == "2") << p_client->address;
}

TEST_F(raft_send, vote_request)
{
    details::context& ctx = *m_p_ctx;

    const raft::server_id_t dst_id = 2;
    tests::empty_client::ptr p_client = client(dst_id);
    EXPECT_FALSE(p_client);

    details::utils::send_vote_request(ctx, dst_id, "2", 1, true, 1, 1);
    wait(dst_id);
    p_client = client(dst_id);
    ASSERT_TRUE(p_client);
    EXPECT_TRUE(p_client->address == "2") << p_client->address;
}

TEST_F(raft_send, vote_response)
{
    details::context& ctx = *m_p_ctx;

    const raft::server_id_t dst_id = 2;
    tests::empty_client::ptr p_client = client(dst_id);
    EXPECT_FALSE(p_client);

    details::utils::send_vote_response(ctx, dst_id, "2", 1, false, true);
    wait(dst_id);
    p_client = client(dst_id);
    ASSERT_TRUE(p_client);
    EXPECT_TRUE(p_client->address == "2") << p_client->address;
}

int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
