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

#ifndef _LIBS_RAFT_CONNECTION_SEND_H_
#define _LIBS_RAFT_CONNECTION_SEND_H_

#include <cstdint>
#include <memory>
#include <shared_mutex>
#include <vector>

#include "raft/io.h"
#include "raft/details/context.h"
#include "raft/details/connection/messages.h"
#include "raft/details/connection/peer.h"
#include "raft/details/connection/serialization.h"

namespace wstux {
namespace raft {
namespace details {
namespace utils {

template<message_type TMsgType> struct message_filler;

template<> struct message_filler<message_type::append_entries_request>
{
    static void fill(message& msg, index_t log_index, term_t log_term, index_t commit, entry::list&& entries)
    {
        msg.append_entries_req.prev_log_index = log_index;
        msg.append_entries_req.prev_log_term = log_term;
        msg.append_entries_req.leader_commit = commit;
        msg.append_entries_req.entries.swap(entries);
    }
};

template<> struct message_filler<message_type::append_entries_response>
{
    static void fill(message& msg, bool accept, index_t last_log_index)
    {
        msg.append_entries_resp.accept = accept;
        msg.append_entries_resp.last_log_index = last_log_index;
    }
};

template<> struct message_filler<message_type::snapshot_request>
{
    static void fill(message& msg, raft::snapshot&& sh)
    {
        msg.snapshot_req.last_index = sh.index;
        msg.snapshot_req.last_term = sh.term;

        msg.snapshot_req.conf = std::move(sh.conf);
        msg.snapshot_req.conf_index = sh.conf_index;

        msg.snapshot_req.buffer.swap(sh.buffer);
    }
};

template<> struct message_filler<message_type::vote_request>
{
    static void fill(message& msg, bool is_prevote, index_t last_log_index, term_t last_log_term)
    {
        msg.vote_req.is_prevote = is_prevote;
        msg.vote_req.last_log_index = last_log_index;
        msg.vote_req.last_log_term = last_log_term;
    }
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
void send(io::ptr p_io, server_id_t dst_id, std::string_view addr, term_t term, server_id_t src_id, TArgs&&... args)
{
    message msg(TMsgType);

    msg.src_id = src_id;
    msg.dst_id = dst_id;
    msg.term = term;

    message_filler<TMsgType>::fill(msg, std::forward<TArgs>(args)...);

    p_io->send(dst_id, addr, serialize(msg));
}

template<message_type TMsgType, typename... TArgs>
inline void send_async(context& ctx, server_id_t dst_id, std::string address, TArgs&&... args)
{
    assert(ctx.id != dst_id);
    ctx.schd.execute_async([p_io = ctx.p_io, dst_id, addr = std::move(address), args...]() mutable -> void {
        send<TMsgType>(std::move(p_io), dst_id, addr, std::move(args)...);
    });
}

inline void send_append_entries_request(context& ctx, const peer& p, const term_t term, const index_t log_index,
                                        const term_t log_term, const index_t commit, entry::list&& entries)
{
    send_async<message_type::append_entries_request>(ctx, p.id, p.address, term, ctx.id, log_index, log_term, commit, std::move(entries));
}

inline void send_append_entries_request(context& ctx, server_id_t dst_id, const term_t term, const index_t log_index,
                                        const term_t log_term, const index_t commit, entry::list&& entries)
{
    const peer::ptr p_peer = peers::find(ctx, dst_id);
    if (p_peer != nullptr) {
        send_append_entries_request(ctx, *p_peer, term, log_index, log_term, commit, std::move(entries));
    } else {
        RAFT_LOG_WARN(ctx, "Server %llu does not exists", dst_id);
    }
}

inline void send_append_entries_response(context& ctx, const peer& p, const term_t term,
                                         const bool accept, const index_t last_log_index)
{
    send_async<message_type::append_entries_response>(ctx, p.id, p.address, term, ctx.id, accept, last_log_index);
}

inline void send_append_entries_response(context& ctx, server_id_t dst_id, const term_t term,
                                         const bool accept, const index_t last_log_index)
{
    const peer::ptr p_peer = peers::find(ctx, dst_id);
    if (p_peer != nullptr) {
        send_append_entries_response(ctx, *p_peer, term, accept, last_log_index);
    } else {
        RAFT_LOG_WARN(ctx, "Server %llu does not exists", dst_id);
    }
}

inline void send_snapshot_request(context& ctx, const peer& p, const term_t term, raft::snapshot&& sh)
{
    send_async<message_type::snapshot_request>(ctx, p.id, p.address, term, ctx.id, std::move(sh));
}

inline void send_snapshot_request(context& ctx, server_id_t dst_id, const term_t term, raft::snapshot&& sh)
{
    const peer::ptr p_peer = peers::find(ctx, dst_id);
    if (p_peer != nullptr) {
        send_snapshot_request(ctx, *p_peer, term, std::move(sh));
    } else {
        RAFT_LOG_WARN(ctx, "Server %llu does not exists", dst_id);
    }
}

inline void send_vote_request(context& ctx, const peer& p, const term_t term, const bool is_prevote,
                              const index_t last_log_index, const term_t last_log_term)
{
    send_async<message_type::vote_request>(ctx, p.id, p.address, term, ctx.id, is_prevote, last_log_index, last_log_term);
}

inline void send_vote_response(context& ctx, const peer& p, const term_t term, const bool is_prevote, const bool accept)
{
    send_async<message_type::vote_response>(ctx, p.id, p.address, term, ctx.id, is_prevote, accept);
}

} // namespace utils
} // namespace details
} // namespace raft
} // namespace wstux

#endif /* _LIBS_RAFT_CONNECTION_SEND_H_ */
