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

#ifndef _LIBS_RAFT_LEADER_ELECTION_CONNECTION_SEND_H_
#define _LIBS_RAFT_LEADER_ELECTION_CONNECTION_SEND_H_

#include <cstdint>
#include <memory>
#include <shared_mutex>
#include <vector>

#include "raft_le/io.h"
#include "raft_le/details/connection/messages.h"
#include "raft_le/details/connection/serialization.h"

namespace wstux {
namespace raft {
namespace le {
namespace details {

template<message_type TMsgType> struct message_filler;

template<> struct message_filler<message_type::heartbeat_request>
{
    static void fill(message& /*msg*/) {}
};

template<> struct message_filler<message_type::heartbeat_response>
{
    static void fill(message& msg, bool accept) { msg.heartbeat_resp.accept = accept; }
};

template<> struct message_filler<message_type::vote_request>
{
    static void fill(message& msg, bool is_prevote) { msg.vote_req.is_prevote = is_prevote; }
};

template<> struct message_filler<message_type::vote_response>
{
    static void fill(message& msg, bool is_prevote, bool accept)
    {
        msg.vote_resp.is_prevote = is_prevote;
        msg.vote_resp.accept = accept;
    }
};

template<message_type TMsgType, typename... TArgs>
void send(iclient::ptr p_client, uint64_t term, int32_t src_id, int32_t dst_id, TArgs&&... args)
{
    message msg;

    msg.type = TMsgType;
    msg.src_id = src_id;
    msg.dst_id = dst_id;
    msg.term = term;

    message_filler<TMsgType>::fill(msg, std::forward<TArgs>(args)...);

    buffer_data_type buffer;
    p_client->send(serialize(msg, buffer));
}

} // namespace details
} // namespace le
} // namespace raft
} // namespace wstux

#endif /* _LIBS_RAFT_LEADER_ELECTION_CONNECTION_SEND_H_ */
