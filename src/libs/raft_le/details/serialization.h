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

#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/iostreams/device/back_inserter.hpp>
#include <boost/serialization/access.hpp>
//#include <boost/serialization/nvp.hpp>
//#include <boost/serialization/shared_ptr.hpp>
#include <boost/serialization/vector.hpp>

#include "raft_le/details/connection/messages.h"

namespace boost {
namespace serialization {
namespace version_1 {

template<typename TArch>
void serialize(TArch& ar, ::wstux::raft::le::details::message& msg, const unsigned int /*version*/)
{
    ar & msg.type;

    ar & msg.src_id;
    ar & msg.dst_id;
    ar & msg.term;

    if (TArch::is_loading::value) {
        msg.init();
    }
    if (msg.type == ::wstux::raft::le::details::message_type::heartbeat_request) {
        ar & msg.heartbeat_req.last_term;
    } else if (msg.type == ::wstux::raft::le::details::message_type::heartbeat_response) {
        ar & msg.heartbeat_resp.accept;
    } else if (msg.type == ::wstux::raft::le::details::message_type::vote_request) {
        ar & msg.vote_req.is_prevote;
        ar & msg.vote_req.last_term;
    } else if (msg.type == ::wstux::raft::le::details::message_type::vote_response) {
        ar & msg.vote_resp.is_prevote;
        ar & msg.vote_resp.accept;
    }
}

} // namespace ver_1

template<typename TArch>
void serialize(TArch& ar, ::wstux::raft::le::server_config& cfg, const unsigned int /*version*/)
{
    ar & cfg.id;
    ar & cfg.endpoint;
    ar & cfg.is_voter;
}

template<typename TArch>
void serialize(TArch& ar, ::wstux::raft::le::details::message& msg, const unsigned int version)
{
    size_t msg_version = ::wstux::raft::le::details::message::version;
    ar & msg_version;

    if (msg_version == ::wstux::raft::le::details::message_version::v_1) {
        version_1::serialize<TArch>(ar, msg, version);
    }
}

} // namespace serialization
} // namespace boost

namespace wstux {
namespace raft {
namespace le {
namespace details {

template<typename T>
void deserialize(const buffer_type& buffer, T& data)
{
    using iostream_type = boost::iostreams::stream<boost::iostreams::array_source>;

    // Create an input stream from the vector's data and size using an array_source device
    iostream_type sin(buffer.data(), buffer.size());

    // Create a binary input archive and deserialize the data
    boost::archive::binary_iarchive arch(sin);
    arch >> data;
}

template<typename T>
T deserialize(const buffer_type& buffer)
{
    T data;

    deserialize(buffer, data);
    return data;
}

template<typename T>
void serialize(const T& data, buffer_type& buffer)
{
    using inserter_type = boost::iostreams::back_insert_device<buffer_type>;
    using iostream_type = boost::iostreams::stream<inserter_type>;

    iostream_type sout{inserter_type(buffer)};

    // Create a binary output archive and serialize the data
    boost::archive::binary_oarchive arch(sout);

    arch << data;

    // Flush the stream to ensure all data is written to the vector
    sout.flush();
}

template<typename T>
buffer_type serialize(const T& data)
{
    buffer_type buffer;

    serialize<T>(data, buffer);
    return buffer;
}

} // namespace details
} // namespace le
} // namespace raft
} // namespace wstux

#endif /* _LIBS_RAFT_LEADER_ELECTION_SERIALIZATION_H_ */
