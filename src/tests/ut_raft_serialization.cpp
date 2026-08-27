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

#include <gtest/gtest.h>

#include "raft/details/connection/serialization.h"

namespace {

bool operator==(const ::wstux::raft::details::message& lhs, const ::wstux::raft::details::message& rhs)
{
    namespace raft = ::wstux::raft::details;

    bool is_eq = true;
    is_eq = is_eq && (lhs.type == rhs.type);
    is_eq = is_eq && (lhs.src_id == rhs.src_id);
    is_eq = is_eq && (lhs.dst_id == rhs.dst_id);
    is_eq = is_eq && (lhs.term == rhs.term);

    if (is_eq && lhs.type == raft::message_type::heartbeat_response) {
        is_eq = is_eq && (lhs.heartbeat_resp.accept == rhs.heartbeat_resp.accept);
    } else if (is_eq && lhs.type == raft::message_type::vote_request) {
        is_eq = is_eq && (lhs.vote_req.is_prevote == rhs.vote_req.is_prevote);
    } else if (is_eq && lhs.type == raft::message_type::vote_response) {
        is_eq = is_eq && (lhs.vote_resp.is_prevote == rhs.vote_resp.is_prevote);
        is_eq = is_eq && (lhs.vote_resp.accept == rhs.vote_resp.accept);
    }
    return is_eq;
}

} // <anonymous> namespace

TEST(raft_serialization, heartbeat_request)
{
    namespace raft = ::wstux::raft;

    raft::details::message msg;
    msg.type = raft::details::message_type::heartbeat_request;
    msg.src_id = 1;
    msg.dst_id = 2;
    msg.term = 1;

    raft::buffer_type buffer = raft::details::serialize(msg);
    raft::details::message dsr_msg = raft::details::deserialize<raft::details::message>(buffer);
    EXPECT_TRUE(msg == dsr_msg);
}

TEST(raft_serialization, heartbeat_response)
{
    namespace raft = ::wstux::raft;

    raft::details::message msg;
    msg.type = raft::details::message_type::heartbeat_response;
    msg.src_id = 1;
    msg.dst_id = 2;
    msg.term = 1;
    msg.heartbeat_resp.accept = true;

    raft::buffer_type buffer = raft::details::serialize(msg);
    raft::details::message dsr_msg = raft::details::deserialize<raft::details::message>(buffer);
    EXPECT_TRUE(msg == dsr_msg);
}

TEST(raft_serialization, vote_request)
{
    namespace raft = ::wstux::raft;

    raft::details::message msg;
    msg.type = raft::details::message_type::vote_request;
    msg.src_id = 1;
    msg.dst_id = 2;
    msg.term = 1;
    msg.vote_req.is_prevote = true;

    raft::buffer_type buffer = raft::details::serialize(msg);
    raft::details::message dsr_msg = raft::details::deserialize<raft::details::message>(buffer);
    EXPECT_TRUE(msg == dsr_msg);
}

TEST(raft_serialization, vote_response)
{
    namespace raft = ::wstux::raft;

    raft::details::message msg;
    msg.type = raft::details::message_type::vote_response;
    msg.src_id = 1;
    msg.dst_id = 2;
    msg.term = 1;
    msg.vote_resp.is_prevote = false;
    msg.vote_resp.accept = true;

    raft::buffer_type buffer = raft::details::serialize(msg);
    raft::details::message dsr_msg = raft::details::deserialize<raft::details::message>(buffer);
    EXPECT_TRUE(msg == dsr_msg);
}

/*TEST(raft_serialization, invalid_buffer)
{
    namespace raft = ::wstux::raft;

    raft::buffer_type buffer(raft::details::message::size / 2, 1);
    EXPECT_TRUE(buffer.size() != raft::details::message::size) << buffer.size();

    raft::details::message dsr_msg = raft::details::deserialize(buffer);
    EXPECT_TRUE(dsr_msg.type == raft::details::message_type::invalid);
}*/

int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
