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

#include <algorithm>
#include <chrono>

#include "raft_le/details/context.h"
#include "raft_le/details/connection/peer.h"
#include "raft_le/details/connection/send.h"
#include "raft_le/details/connection/serialization.h"

namespace wstux {
namespace raft {
namespace le {
namespace details {

////////////////////////////////////////////////////////////////////////////////
// class peer

peer::peer(const server_config& cfg, const io::ptr& p_io, size_t hb_expired_interval_ms)
    : m_cfg(cfg)
    , m_heartbeat_expired_interval_ms(hb_expired_interval_ms)
    , m_p_client(p_io->create_client(m_cfg.id, m_cfg.endpoint))
    , m_last_response_ms(0)
    , m_recent_recv(false)
{}

bool peer::is_probe_expired() const
{
    if (m_last_response_ms == 0) {
        return false;
    }
    return (utils::current_time_ms() - m_last_response_ms > m_heartbeat_expired_interval_ms);
}

void peer::send_heartbeat_request(uint64_t term, int32_t src_id)
{
    send<message_type::heartbeat_request>(m_p_client, term, src_id, m_cfg.id);
}

void peer::send_heartbeat_response(uint64_t term, int32_t src_id, bool accept)
{
    send<message_type::heartbeat_response>(m_p_client, term, src_id, m_cfg.id, accept);
}

void peer::send_vote_request(uint64_t term, int32_t src_id, bool is_prevote)
{
    send<message_type::vote_request>(m_p_client, term, src_id, m_cfg.id, is_prevote);
}

void peer::send_vote_response(uint64_t term, int32_t src_id, bool is_prevote, bool accept)
{
    send<message_type::vote_response>(m_p_client, term, src_id, m_cfg.id, is_prevote, accept);
}

void peer::update(size_t hb_expired_interval_ms)
{
    m_heartbeat_expired_interval_ms = hb_expired_interval_ms;
}

void peer::update_last_response()
{
    m_last_response_ms = utils::current_time_ms();
}

} // namespace details
} // namespace le
} // namespace raft
} // namespace wstux
