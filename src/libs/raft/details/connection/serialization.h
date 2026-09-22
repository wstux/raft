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

#ifndef _LIBS_RAFT_SERIALIZATION_H_
#define _LIBS_RAFT_SERIALIZATION_H_

#include <cassert>
#include <type_traits>
#include <utility>

#include <boost/endian/conversion.hpp>

#include "raft/io.h"
#include "raft/details/connection/messages.h"

namespace wstux {
namespace raft {
namespace details {

enum serialize_version : uint32_t
{
    sv_1 = 20260921,
    current = sv_1
};

namespace v1 {

template<typename T>
inline void read(T& value, const char*& p_buffer)
{
    std::memcpy(&value, p_buffer, sizeof(T));
    value = boost::endian::big_to_native(value);
    p_buffer += sizeof(T);
}

template<typename T>
inline void write(const T& value, char*& p_buffer)
{
    T temp = boost::endian::native_to_big(value);
    std::memcpy(p_buffer, &temp, sizeof(T));
    p_buffer += sizeof(T);
}

template<typename T>
constexpr size_t size(const T&) { return sizeof(T); }

template<>
inline void read<bool>(bool& value, const char*& p_buffer)
{
    uint8_t byte;
    std::memcpy(&byte, p_buffer, 1);
    p_buffer += 1;
    value = (byte != 0);
}

template<>
inline void write<bool>(const bool& value, char*& p_buffer)
{
    uint8_t byte = value ? 1 : 0;
    std::memcpy(p_buffer, &byte, 1);
    p_buffer += 1;
}

template<>
constexpr size_t size<bool>(const bool&) { return 1; }

template<>
inline void read<entry_type>(entry_type& type, const char*& p_buffer)
{
    static_assert(sizeof(int32_t) == sizeof(entry_type));
    static_assert(std::is_same<int32_t, std::underlying_type_t<entry_type>>::value);

    int32_t raw_type;
    read<int32_t>(raw_type, p_buffer);
    type = static_cast<entry_type>(raw_type);
}

template<>
inline void write<entry_type>(const entry_type& type, char*& p_buffer)
{
    static_assert(sizeof(int32_t) == sizeof(entry_type));
    static_assert(std::is_same<int32_t, std::underlying_type_t<entry_type>>::value);

    const int32_t raw_type = static_cast<int32_t>(type);
    write<int32_t>(raw_type, p_buffer);
}

template<>
inline void read<message_type>(message_type& type, const char*& p_buffer)
{
    static_assert(sizeof(int32_t) == sizeof(message_type));
    static_assert(std::is_same<int32_t, std::underlying_type_t<message_type>>::value);

    int32_t raw_type;
    read<int32_t>(raw_type, p_buffer);
    type = static_cast<message_type>(raw_type);
}

template<>
inline void write<message_type>(const message_type& type, char*& p_buffer)
{
    static_assert(sizeof(int32_t) == sizeof(message_type));
    static_assert(std::is_same<int32_t, std::underlying_type_t<message_type>>::value);

    const int32_t raw_type = static_cast<int32_t>(type);
    write<int32_t>(raw_type, p_buffer);
}

template<>
inline void read<message_version>(message_version& version, const char*& p_buffer)
{
    static_assert(sizeof(uint32_t) == sizeof(message_version));
    static_assert(std::is_same<uint32_t, std::underlying_type_t<message_version>>::value);

    uint32_t raw_version;
    read<uint32_t>(raw_version, p_buffer);
    version = static_cast<message_version>(raw_version);
}

template<>
inline void write<message_version>(const message_version& version, char*& p_buffer)
{
    static_assert(sizeof(uint32_t) == sizeof(message_version));
    static_assert(std::is_same<uint32_t, std::underlying_type_t<message_version>>::value);

    const uint32_t raw_version = static_cast<uint32_t>(version);
    write<uint32_t>(raw_version, p_buffer);
}

template<>
inline void read<serialize_version>(serialize_version& version, const char*& p_buffer)
{
    static_assert(sizeof(uint32_t) == sizeof(serialize_version));
    static_assert(std::is_same<uint32_t, std::underlying_type_t<serialize_version>>::value);

    uint32_t raw_version;
    read<uint32_t>(raw_version, p_buffer);
    version = static_cast<serialize_version>(raw_version);
}

template<>
inline void write<serialize_version>(const serialize_version& version, char*& p_buffer)
{
    static_assert(sizeof(uint32_t) == sizeof(serialize_version));
    static_assert(std::is_same<uint32_t, std::underlying_type_t<serialize_version>>::value);

    const uint32_t raw_version = static_cast<uint32_t>(version);
    write<uint32_t>(raw_version, p_buffer);
}

template<>
inline void read<buffer_type>(buffer_type& buffer, const char*& p_buffer)
{
    uint64_t size = 0;
    read<uint64_t>(size, p_buffer);
    if (size != 0) {
        buffer.resize(size);
        std::memcpy(buffer.data(), p_buffer, size);
        p_buffer += size;
    }
}

template<>
inline void write<buffer_type>(const buffer_type& buffer, char*& p_buffer)
{
    const uint64_t size = static_cast<uint64_t>(buffer.size());
    write<uint64_t>(size, p_buffer);
    if (size != 0) {
        std::memcpy(p_buffer, buffer.data(), size);
        p_buffer += size;
    }
}

template<>
inline size_t size<buffer_type>(const buffer_type& buffer)
{
    return size<uint64_t>(buffer.size()) + (buffer.size() * sizeof(buffer_type::value_type));
}

template<>
inline void read<std::string>(std::string& str, const char*& p_buffer)
{
    uint64_t size = 0;
    read<uint64_t>(size, p_buffer);
    if (size != 0) {
        str.resize(size);
        std::memcpy(str.data(), p_buffer, size);
        p_buffer += size;
    }
}

template<>
inline void write<std::string>(const std::string& str, char*& p_buffer)
{
    const uint64_t size = static_cast<uint64_t>(str.size());
    write<uint64_t>(size, p_buffer);
    if (size != 0) {
        std::memcpy(p_buffer, str.data(), size);
        p_buffer += size;
    }
}

template<>
inline size_t size<std::string>(const std::string& str) { return size<uint64_t>(str.size()) + (str.size() * sizeof(std::string::value_type)); }

template<>
inline void read<entry::ptr>(entry::ptr& p_entry, const char*& p_buffer)
{
    assert(p_entry == nullptr);
    p_entry = std::make_shared<entry>();
    read<term_t>(p_entry->term, p_buffer);
    read<entry_type>(p_entry->type, p_buffer);
    read<buffer_type>(p_entry->buffer, p_buffer);
}

template<>
inline void write(const entry::ptr& p_entry, char*& p_buffer)
{
    assert(p_entry != nullptr);
    write<term_t>(p_entry->term, p_buffer);
    write<entry_type>(p_entry->type, p_buffer);
    write<buffer_type>(p_entry->buffer, p_buffer);
}

template<>
inline size_t size(const entry::ptr& p_entry)
{
    assert(p_entry != nullptr);

    size_t full_size = 0;
    full_size += size<term_t>(p_entry->term);
    full_size += size<entry_type>(p_entry->type);
    full_size += v1::size<buffer_type>(p_entry->buffer);
    return full_size;
}

template<>
inline void read<entry::list>(entry::list& entries, const char*& p_buffer)
{
    uint64_t size = 0;
    read<uint64_t>(size, p_buffer);
    if (size != 0) {
        entries.resize(size);
        for (uint64_t i = 0; i < size; ++i) {
            read<entry::ptr>(entries[i], p_buffer);
        }
    }
}

template<>
inline void write<entry::list>(const entry::list& entries, char*& p_buffer)
{
    const uint64_t size = static_cast<uint64_t>(entries.size());
    write<uint64_t>(size, p_buffer);
    if (size != 0) {
        for (uint64_t i = 0; i < size; ++i) {
            write<entry::ptr>(entries[i], p_buffer);
        }
    }
}

template<>
inline size_t size<entry::list>(const entry::list& entries)
{
    size_t full_size = size<uint64_t>(entries.size());
    if (entries.size() != 0) {
        for (size_t i = 0; i < entries.size(); ++i) {
            full_size += size<entry::ptr>(entries[i]);
        }
    }
    return full_size;
}

template<>
inline void read<server_config>(server_config& cfg, const char*& p_buffer)
{
    read<server_id_t>(cfg.id, p_buffer);
    read<std::string>(cfg.address, p_buffer);
    read<bool>(cfg.is_voter, p_buffer);
}

template<>
inline void write<server_config>(const server_config& val, char*& p_buffer)
{
    write<server_id_t>(val.id, p_buffer);
    write<std::string>(val.address, p_buffer);
    write<bool>(val.is_voter, p_buffer);
}

template<>
inline size_t size<server_config>(const server_config& val)
{
    size_t full_size = 0;
    full_size += size<server_id_t>(val.id);
    full_size += v1::size<std::string>(val.address);
    full_size += size<bool>(val.is_voter);
    return full_size;
}

template<>
inline void read<std::vector<server_config>>(std::vector<server_config>& servers, const char*& p_buffer)
{
    uint64_t size = 0;
    read<uint64_t>(size, p_buffer);
    if (size != 0) {
        servers.resize(size);
        for (uint64_t i = 0; i < size; ++i) {
            read<server_config>(servers[i], p_buffer);
        }
    }
}

template<>
inline void write<std::vector<server_config>>(const std::vector<server_config>& servers, char*& p_buffer)
{
    const uint64_t size = static_cast<uint64_t>(servers.size());
    write<uint64_t>(size, p_buffer);
    if (size != 0) {
        for (uint64_t i = 0; i < size; ++i) {
            write<server_config>(servers[i], p_buffer);
        }
    }
}

template<>
inline size_t size<std::vector<server_config>>(const std::vector<server_config>& servers)
{
    size_t full_size = size<uint64_t>(servers.size());
    if (servers.size() != 0) {
        for (size_t i = 0; i < servers.size(); ++i) {
            full_size += size<server_config>(servers[i]);
        }
    }
    return full_size;
}

template<>
inline void read<cluster_config>(cluster_config& cfg, const char*& p_buffer)
{
    read<std::vector<server_config>>(cfg.servers, p_buffer);
}

template<>
inline void write<cluster_config>(const cluster_config& cfg, char*& p_buffer) { write<std::vector<server_config>>(cfg.servers, p_buffer); }

template<>
inline size_t size<cluster_config>(const cluster_config& cfg) { return v1::size<std::vector<server_config>>(cfg.servers); }

template<>
inline void read<message>(message& msg, const char*& p_buffer)
{
    uint32_t version = 0;
    read<uint32_t>(version, p_buffer);
    if (version == message_version::v_1) {
        read<server_id_t>(msg.src_id, p_buffer);
        read<server_id_t>(msg.dst_id, p_buffer);
        read<std::string>(msg.address, p_buffer);
        read<term_t>(msg.term, p_buffer);

        if (msg.type == ::wstux::raft::details::message_type::append_entries_request) {
            read<index_t>(msg.append_entries_req.prev_log_index, p_buffer);
            read<term_t>(msg.append_entries_req.prev_log_term, p_buffer);
            read<index_t>(msg.append_entries_req.leader_commit, p_buffer);
            read<entry::list>(msg.append_entries_req.entries, p_buffer);
        } else if (msg.type == ::wstux::raft::details::message_type::append_entries_response) {
            read<bool>(msg.append_entries_resp.accept, p_buffer);
            read<index_t>(msg.append_entries_resp.last_log_index, p_buffer);
        } else if (msg.type == ::wstux::raft::details::message_type::snapshot_request) {
            read<index_t>(msg.snapshot_req.last_index, p_buffer);
            read<term_t>(msg.snapshot_req.last_term, p_buffer);
            read<cluster_config>(msg.snapshot_req.conf, p_buffer);
            read<index_t>(msg.snapshot_req.conf_index, p_buffer);
            read<buffer_type>(msg.snapshot_req.buffer, p_buffer);
        } else if (msg.type == ::wstux::raft::details::message_type::vote_request) {
            read<bool>(msg.vote_req.is_prevote, p_buffer);
            read<index_t>(msg.vote_req.last_log_index, p_buffer);
            read<term_t>(msg.vote_req.last_log_term, p_buffer);
        } else if (msg.type == ::wstux::raft::details::message_type::vote_response) {
            read<bool>(msg.vote_resp.is_prevote, p_buffer);
            read<bool>(msg.vote_resp.accept, p_buffer);
        }
    }
}

template<>
inline void write<message>(const message& msg, char*& p_buffer)
{
    write<uint32_t>(message::version, p_buffer);
    if (message::version == message_version::v_1) {
        write<server_id_t>(msg.src_id, p_buffer);
        write<server_id_t>(msg.dst_id, p_buffer);
        write<std::string>(msg.address, p_buffer);
        write<term_t>(msg.term, p_buffer);

        if (msg.type == ::wstux::raft::details::message_type::append_entries_request) {
            write<index_t>(msg.append_entries_req.prev_log_index, p_buffer);
            write<term_t>(msg.append_entries_req.prev_log_term, p_buffer);
            write<index_t>(msg.append_entries_req.leader_commit, p_buffer);
            write<entry::list>(msg.append_entries_req.entries, p_buffer);
        } else if (msg.type == ::wstux::raft::details::message_type::append_entries_response) {
            write<bool>(msg.append_entries_resp.accept, p_buffer);
            write<index_t>(msg.append_entries_resp.last_log_index, p_buffer);
        } else if (msg.type == ::wstux::raft::details::message_type::snapshot_request) {
            write<index_t>(msg.snapshot_req.last_index, p_buffer);
            write<term_t>(msg.snapshot_req.last_term, p_buffer);
            write<cluster_config>(msg.snapshot_req.conf, p_buffer);
            write<index_t>(msg.snapshot_req.conf_index, p_buffer);
            write<buffer_type>(msg.snapshot_req.buffer, p_buffer);
        } else if (msg.type == ::wstux::raft::details::message_type::vote_request) {
            write<bool>(msg.vote_req.is_prevote, p_buffer);
            write<index_t>(msg.vote_req.last_log_index, p_buffer);
            write<term_t>(msg.vote_req.last_log_term, p_buffer);
        } else if (msg.type == ::wstux::raft::details::message_type::vote_response) {
            write<bool>(msg.vote_resp.is_prevote, p_buffer);
            write<bool>(msg.vote_resp.accept, p_buffer);
        }
    }
}

template<>
inline size_t size<message>(const message& msg)
{
    size_t full_size = 0;
    full_size += size<message_type>(msg.type);
    full_size += size<uint32_t>(message::version);

    if (message::version == message_version::v_1) {
        full_size += size<server_id_t>(msg.src_id);
        full_size += size<server_id_t>(msg.dst_id);
        full_size += v1::size<std::string>(msg.address);
        full_size += size<term_t>(msg.term);

        if (msg.type == ::wstux::raft::details::message_type::append_entries_request) {
            full_size += size<index_t>(msg.append_entries_req.prev_log_index);
            full_size += size<term_t>(msg.append_entries_req.prev_log_term);
            full_size += size<index_t>(msg.append_entries_req.leader_commit);
            full_size += v1::size<entry::list>(msg.append_entries_req.entries);
        } else if (msg.type == ::wstux::raft::details::message_type::append_entries_response) {
            full_size += size<bool>(msg.append_entries_resp.accept);
            full_size += size<index_t>(msg.append_entries_resp.last_log_index);
        } else if (msg.type == ::wstux::raft::details::message_type::snapshot_request) {
            full_size += size<index_t>(msg.snapshot_req.last_index);
            full_size += size<term_t>(msg.snapshot_req.last_term);
            full_size += size<cluster_config>(msg.snapshot_req.conf);
            full_size += size<index_t>(msg.snapshot_req.conf_index);
            full_size += v1::size<buffer_type>(msg.snapshot_req.buffer);
        } else if (msg.type == ::wstux::raft::details::message_type::vote_request) {
            full_size += size<bool>(msg.vote_req.is_prevote);
            full_size += size<index_t>(msg.vote_req.last_log_index);
            full_size += size<term_t>(msg.vote_req.last_log_term);
        } else if (msg.type == ::wstux::raft::details::message_type::vote_response) {
            full_size += size<bool>(msg.vote_resp.is_prevote);
            full_size += size<bool>(msg.vote_resp.accept);
        }
    }
    return full_size;
}

template<typename T>
inline T deserialize(const char*& p_buffer)
{
    T value;
    read<T>(value, p_buffer);
    return value;
}

template<typename T>
inline void deserialize(T& data, const char*& p_buffer)
{
    read<T>(data, p_buffer);
}

template<>
inline message deserialize<message>(const char*& p_buffer)
{
    message_type type = message_type::invalid;
    read<message_type>(type, p_buffer);

    message msg(type);
    read<message>(msg, p_buffer);
    return msg;
}

template<typename T>
inline void serialize(const T& data, buffer_type& buffer)
{
    assert(buffer.empty());

    const size_t full_size = size<T>(data) + size<serialize_version>(serialize_version::sv_1);
    buffer.resize(full_size);
    buffer_type::value_type* p_buffer = buffer.data();
    write<serialize_version>(serialize_version::sv_1, p_buffer);
    write<T>(data, p_buffer);
}

template<>
inline void serialize<message>(const message& msg, buffer_type& buffer)
{
    assert(buffer.empty());

    const size_t full_size = size<message>(msg) + size<uint32_t>(message::version) + size<serialize_version>(serialize_version::sv_1);
    buffer.resize(full_size);
    buffer_type::value_type* p_buffer = buffer.data();
    write<serialize_version>(serialize_version::sv_1, p_buffer);

    write<message_type>(msg.type, p_buffer);
    write<message>(msg, p_buffer);
}

} // namespace v1

template<typename T, typename TBuffer>
inline T deserialize(const TBuffer& buffer)
{
    const typename TBuffer::value_type* p_buffer = buffer.data();
    const serialize_version v = v1::deserialize<serialize_version>(p_buffer);
    if (v != serialize_version::sv_1) {
        assert(false && "Invalid deserialize version");
    }
    return v1::deserialize<T>(p_buffer);
}

template<typename T, typename TBuffer>
inline void deserialize(const TBuffer& buffer, T& data)
{
    const typename TBuffer::value_type* p_buffer = buffer.data();
    const serialize_version v = v1::deserialize<serialize_version>(p_buffer);
    if (v != serialize_version::sv_1) {
        assert(false && "Invalid deserialize version");
    }
    v1::deserialize<T>(data, p_buffer);
}

template<typename T>
inline buffer_type serialize(const T& data)
{
    buffer_type buffer;
    if (serialize_version::current == serialize_version::sv_1) {
        v1::serialize<T>(data, buffer);
    } else {
        assert(false && "Invalid serialize version");
    }
    return buffer;
}

} // namespace details
} // namespace raft
} // namespace wstux

#endif /* _LIBS_RAFT_SERIALIZATION_H_ */
