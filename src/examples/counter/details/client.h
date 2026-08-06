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

#ifndef _EXAMPLES_RAFT_LEADER_ELECTION_COUNTER_CLIENT_H_
#define _EXAMPLES_RAFT_LEADER_ELECTION_COUNTER_CLIENT_H_

#include <memory>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Woverflow"
#pragma GCC diagnostic ignored "-Wunused-parameter"
    #include <grpc++/grpc++.h>
#pragma GCC diagnostic pop

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Woverflow"
    #include <node.pb.h>
    #include <node.grpc.pb.h>
#pragma GCC diagnostic pop

#include "raft_le/io.h"

#include "counter/details/logging.h"

namespace wstux {
namespace examples {
namespace counter {
namespace details {

class client final : public raft::le::iclient
{
private:
    using context_type = ::grpc::ClientContext;
    using service_type = ::cluster::NodeService;
    using stub_type    = typename service_type::Stub;

public:
    using ptr = std::shared_ptr<client>;

public:
    client(const std::string &endpoint, raft::le::logging_handler::severity_level lvl)
        : m_address(endpoint)
        , m_p_stub(service_type::NewStub(make_channel(m_address)))
        , m_logger(lvl)
    {}

    virtual ~client() {}

    virtual void send(const raft::le::buffer_type& buf) override final
    {
        context_type ctx;

        ::cluster::Message msg;
        msg.set_buffer(buf.data(), buf.size());

        ::cluster::Empty resp;
        ::grpc::Status status = m_p_stub->SendRaftMessage(&ctx, msg, &resp);
        if (! status.ok()) {
            LOG_ERROR(m_logger, "Failed to send append entries request data to server " << m_address);
        }
    }

    void send_counter(const uint64_t counter)
    {
        context_type ctx;

        ::cluster::CounterMessage msg;
        msg.set_counter(counter);

        ::cluster::Empty resp;
        ::grpc::Status status = m_p_stub->SentCounterMessage(&ctx, msg, &resp);
        if (! status.ok()) {
            LOG_ERROR(m_logger, "Failed to send append entries request data to server " << m_address);
        }
    }

private:
    static std::shared_ptr<::grpc::Channel> make_channel(const std::string& addr)
    {
        return ::grpc::CreateChannel(addr, ::grpc::InsecureChannelCredentials());
    }

private:
    const std::string m_address;
    std::unique_ptr<stub_type> m_p_stub;

    logging_handler m_logger;
};

} // namespace details
} // namespace counter
} // namespace examples
} // namespace wstux

#endif /* _EXAMPLES_RAFT_LEADER_ELECTION_COUNTER_CLIENT_H_ */

