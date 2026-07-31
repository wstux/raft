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

#ifndef _LIBS_RAFT_LEADER_ELECTION_SERIALIZATION_H_
#define _LIBS_RAFT_LEADER_ELECTION_SERIALIZATION_H_

#include <cassert>

#include <boost/endian/conversion.hpp>

#include "raft_le/details/connection/messages.h"

namespace wstux {
namespace raft {
namespace le {
namespace details {
namespace v1 {

template<typename TDest, typename TSrc>
void read(TSrc& src, const char*& p_buffer)
{
    TDest value;
    std::memcpy(&value, p_buffer, sizeof(TDest));
    p_buffer += sizeof(TDest);
    src = static_cast<TSrc>(boost::endian::big_to_native(value));
}

template<>
inline void read<uint8_t, bool>(bool& src, const char*& p_buffer)
{
    uint8_t value;
    std::memcpy(&value, p_buffer, 1);
    p_buffer += 1;
    src = (value != 0);
}

template<typename TDest, typename TSrc>
void write(const TSrc& src, char*& p_buffer)
{
    const TDest value = boost::endian::native_to_big(static_cast<TDest>(src));
    std::memcpy(p_buffer, &value, sizeof(TDest));
    p_buffer += sizeof(TDest);
}

template<>
inline void write<uint8_t, bool>(const bool& src, char*& p_buffer)
{
    uint8_t value = src ? 1 : 0;
    std::memcpy(p_buffer, &value, 1);
    p_buffer += 1;
}

inline void deserialize(const char* p_buffer, message& msg)
{
    read<int32_t>(msg.type, p_buffer);
    read<uint64_t>(msg.src_id, p_buffer);
    read<uint64_t>(msg.dst_id, p_buffer);
    read<uint32_t>(msg.term, p_buffer);

    if (msg.type == message_type::heartbeat_response) {
        read<uint8_t>(msg.heartbeat_resp.accept, p_buffer);
    } else if (msg.type == message_type::vote_request) {
        read<uint8_t>(msg.vote_req.is_prevote, p_buffer);
    } else if (msg.type == message_type::vote_response) {
        read<uint8_t>(msg.vote_resp.is_prevote, p_buffer);
        read<uint8_t>(msg.vote_resp.accept, p_buffer);
    }
}

inline void serialize(const message& msg, char* p_buffer)
{
    static_assert(sizeof(int32_t) == sizeof(message_type));
    static_assert(std::is_same<uint64_t, server_id_t>::value);
    static_assert(std::is_same<uint32_t, term_t>::value);

    write<int32_t>(msg.type, p_buffer);
    write<uint64_t>(msg.src_id, p_buffer);
    write<uint64_t>(msg.dst_id, p_buffer);
    write<uint32_t>(msg.term, p_buffer);

    if (msg.type == message_type::heartbeat_response) {
        write<uint8_t>(msg.heartbeat_resp.accept, p_buffer);
    } else if (msg.type == message_type::vote_request) {
        write<uint8_t>(msg.vote_req.is_prevote, p_buffer);
    } else if (msg.type == message_type::vote_response) {
        write<uint8_t>(msg.vote_resp.is_prevote, p_buffer);
        write<uint8_t>(msg.vote_resp.accept, p_buffer);
    }
}

} // namespace v1

inline void deserialize(const buffer_type& buffer, message& msg)
{
    assert(buffer.size() <= message::size);

    const char* p_buffer = buffer.data();

    uint32_t version = 0;
    v1::read<uint32_t>(version, p_buffer);
    if (version == message_version::v_1) {
        v1::deserialize(p_buffer, msg);
    }
}

inline message deserialize(const buffer_type& buffer)
{
    message msg;

    deserialize(buffer, msg);
    return msg;
}

inline void serialize(const message& msg, buffer_type& buffer)
{
    constexpr size_t message_size = sizeof(uint32_t) + sizeof(int32_t) + sizeof(uint64_t) +
        sizeof(uint64_t) + sizeof(uint32_t) + sizeof(uint8_t) + sizeof(uint8_t);
    static_assert(message_size <= message::size,  "Invalid message size");

    buffer.resize(message::size);

    if (message::version == message_version::v_1) {
        char* p_buffer = buffer.data();
        v1::write<uint32_t>(message::version, p_buffer);
        v1::serialize(msg, p_buffer);
    }
}

inline buffer_type serialize(const message& msg)
{
    buffer_type buffer;

    serialize(msg, buffer);
    return buffer;
}

} // namespace details
} // namespace le
} // namespace raft
} // namespace wstux

#endif /* _LIBS_RAFT_LEADER_ELECTION_SERIALIZATION_H_ */
