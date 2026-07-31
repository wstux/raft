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

#ifndef _LIBS_RAFT_LEADER_ELECTION_CONNECTION_MESSAGES_H_
#define _LIBS_RAFT_LEADER_ELECTION_CONNECTION_MESSAGES_H_

#include <type_traits>

#include "raft_le/io.h"

namespace wstux {
namespace raft {
namespace le {
namespace details {

enum message_version : uint32_t
{
    v_1 = 20260727
};

enum message_type : int32_t
{
    heartbeat_request  = 0,
    heartbeat_response = 1,
    vote_request       = 2,
    vote_response      = 3
};

struct heartbeat_message final
{};

struct heartbeat_response_message final
{
    bool accept;
};

struct vote_message final
{
    bool is_prevote;
};

struct vote_response_message final
{
    bool is_prevote;
    bool accept;
};

struct message final
{
    static constexpr size_t size = 32;
    static constexpr uint32_t version = message_version::v_1;

    message_type type;

    server_id_t src_id;
    server_id_t dst_id;
    term_t term;

    union {
        heartbeat_message          heartbeat_req;
        heartbeat_response_message heartbeat_resp;
        vote_message               vote_req;
        vote_response_message      vote_resp;
    };
};

static_assert(std::is_pod<message>::value, "Message struct must be pod");
static_assert(std::is_trivially_copyable<message>::value, "Message struct must be trivially copyable");
static_assert(sizeof(message) == message::size, "Invalid message size");

} // namespace details
} // namespace le
} // namespace raft
} // namespace wstux

#endif /* _LIBS_RAFT_LEADER_ELECTION_CONNECTION_MESSAGES_H_ */
