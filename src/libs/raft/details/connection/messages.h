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

#ifndef _LIBS_RAFT_CONNECTION_MESSAGES_H_
#define _LIBS_RAFT_CONNECTION_MESSAGES_H_

#include <type_traits>

#include "raft/io.h"

namespace wstux {
namespace raft {
namespace details {

enum message_version : uint32_t
{
    v_1 = 20260827
};

enum message_type : int32_t
{
    append_entries_request  = 0,
    append_entries_response = 1,
    snapshot_request        = 2,
    vote_request            = 3,
    vote_response           = 4,
    invalid
};

struct append_entries_message final
{
    index_t prev_log_index;
    term_t prev_log_term;
    index_t leader_commit;

    entry::list entries;
};

struct append_entries_response_message final
{
    bool accept;
    index_t last_log_index;
};

struct snapshot_message
{
    index_t last_index;   //!< Index of last entry in the snapshot.
    term_t last_term;     //!< Term of last_index.

    cluster_config conf;  //!< Config as of last_index.
    index_t conf_index;   //!< Commit index of conf.

    buffer_type buffer;   //!< Raw snapshot data.
};

struct vote_message final
{
    bool is_prevote;
    index_t last_log_index;
    index_t last_log_term;
};

struct vote_response_message final
{
    bool is_prevote;
    bool accept;
};

struct message final
{
    static constexpr size_t version = message_version::v_1;

    message() {}

    explicit message(message_type t)
        : type(t)
    {
        init();
    }

    ~message() { apply<destroy_op>(); }

    message(const message&) = delete;

    message(message&& other) noexcept
        : type(other.type)
        , src_id(other.src_id)
        , dst_id(other.dst_id)
        , term(other.term)
    {
        apply_other<move_op>(other);
        other.type = message_type::invalid;
    }

    message& operator=(const message&) = delete;
    message& operator=(message&&) = delete;

    void init() { apply<construct_op>(); }

    message_type type;

    server_id_t src_id;
    server_id_t dst_id;
    term_t term;

    union {
        append_entries_message          append_entries_req;
        append_entries_response_message append_entries_resp;
        snapshot_message                snapshot_req;
        vote_message                    vote_req;
        vote_response_message           vote_resp;
    };

private:
    struct construct_op
    {
        template<typename T>
        static void run(T& field) { ::new (static_cast<void*>(&field)) T(); }
    };

    struct destroy_op
    {
        template<typename T>
        static void run(T& field) { field.~T(); }
    };

    struct move_op
    {
        template<typename T>
        static void run(T& cur, T& other) { ::new (static_cast<void*>(&cur)) T(std::move(other)); }
    };

private:
    template<typename TAction>
    void apply()
    {
        switch (type) {
        case append_entries_request:  TAction::run(this->*(&message::append_entries_req)); break;
        case append_entries_response: TAction::run(this->*(&message::append_entries_resp)); break;
        case snapshot_request:        TAction::run(this->*(&message::snapshot_req)); break;
        case vote_request:            TAction::run(this->*(&message::vote_req)); break;
        case vote_response:           TAction::run(this->*(&message::vote_resp)); break;
        default:                      break;
        }
    }

    template<typename TAction>
    void apply_other(message& other)
    {
        switch (type) {
        case append_entries_request:  TAction::run(append_entries_req, other.append_entries_req); break;
        case append_entries_response: TAction::run(append_entries_resp, other.append_entries_resp); break;
        case snapshot_request:        TAction::run(snapshot_req, other.snapshot_req); break;
        case vote_request:            TAction::run(vote_req, other.vote_req); break;
        case vote_response:           TAction::run(vote_resp, other.vote_resp); break;
        default:                      break;
        }
    }
};

} // namespace details
} // namespace raft
} // namespace wstux

#endif /* _LIBS_RAFT_CONNECTION_MESSAGES_H_ */
