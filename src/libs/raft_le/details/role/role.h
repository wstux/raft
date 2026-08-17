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

#ifndef _LIBS_RAFT_LEADER_ELECTION_ROLE_ROLE_H_
#define _LIBS_RAFT_LEADER_ELECTION_ROLE_ROLE_H_

#include <cstdint>

#include "raft_le/io.h"

namespace wstux {
namespace raft {
namespace le {
namespace details {
namespace role {

enum role_type : int32_t
{
    undefined = 0,
    follower  = 1,
    candidate = 2,
    leader    = 3
};

struct follower_role final
{
    server_id_t leader_id;
};

struct candidate_role final
{
    size_t votes_granted;
    bool is_prevote;
};

struct leader_role final
{
};

struct state final
{
    inline bool is_follower() const { return role == role_type::follower; }
    inline bool is_candidate() const { return role == role_type::candidate; }
    inline bool is_leader() const { return role == role_type::leader; }

    inline bool has_leader() const { return is_follower() && follower_state.leader_id != gk_invalid_id; }

    const char* str() const
    {
        static const char* p_follower_str = "follower";
        static const char* p_candidate_str = "candidate";
        static const char* p_leader_str = "leader";
        static const char* p_undefined_str = "undefined";

        if (is_follower()) {
            return p_follower_str;
        } else if (is_candidate()) {
            return p_candidate_str;
        } else if (is_leader()) {
            return p_leader_str;
        }
        return p_undefined_str;
    }

    role_type role = role_type::undefined;

    union {
        follower_role  follower_state;
        candidate_role candidate_state;
        leader_role    leader_state;
    };

    bool is_voter;
    server_id_t voted_for;
};

} // namespace role
} // namespace details
} // namespace le
} // namespace raft
} // namespace wstux

#endif /* _LIBS_RAFT_LEADER_ELECTION_ROLE_ROLE_H_ */
