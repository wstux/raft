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

#ifndef _LIBS_RAFT_LEADER_ELECTION_CONNECTION_PEER_H_
#define _LIBS_RAFT_LEADER_ELECTION_CONNECTION_PEER_H_

#include <cstdint>
#include <atomic>
#include <map>
#include <memory>
#include <shared_mutex>
#include <vector>

#include "raft_le/io.h"

namespace wstux {
namespace raft {
namespace le {
namespace details {

class peer final
{
public:
    using ptr = std::shared_ptr<peer>;
    using list = std::vector<ptr>;
    using map = std::map<server_id_t, ptr>;

public:
    peer(const server_config& cfg, const io::ptr& p_io, size_t hb_expired_interval_ms);

    const server_config& config() const { return m_cfg; }

    server_id_t id() const { return m_cfg.id; }

    bool is_probe_expired() const;

    bool is_voter() const { return m_cfg.is_voter; }

    size_t last_response_ms() const { return m_last_response_ms; }

    void mark_recent_recv() { m_recent_recv = true; }

    bool recent_recv() const { return m_recent_recv; }

    bool reset_recent_recv() { return m_recent_recv.exchange(false); }

    void send_heartbeat_request(uint64_t term, int32_t src_id, term_t log_term);

    void send_heartbeat_response(uint64_t term, int32_t src_id, bool accept);

    void send_vote_request(uint64_t term, int32_t src_id, bool is_prevote);

    void send_vote_response(uint64_t term, int32_t src_id, bool is_prevote, bool accept);

    void update_last_response();

private:
    const server_config m_cfg;
    size_t m_heartbeat_expired_interval_ms;
    iclient::ptr m_p_client;

    std::atomic_size_t m_last_response_ms;
    std::atomic_bool m_recent_recv;
};

} // namespace details
} // namespace le
} // namespace raft
} // namespace wstux

#endif /* _LIBS_RAFT_LEADER_ELECTION_CONNECTION_PEER_H_ */
