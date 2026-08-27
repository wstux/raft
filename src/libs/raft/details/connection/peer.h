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

#ifndef _LIBS_RAFT_CONNECTION_PEER_H_
#define _LIBS_RAFT_CONNECTION_PEER_H_

#include <cstdint>
#include <memory>
#include <shared_mutex>
#include <vector>

#include "raft/io.h"

namespace wstux {
namespace raft {
namespace details {

struct peer final
{
    using ptr = peer*;
    using list = std::vector<peer>;

    explicit peer(const server_config& cfg)
        : id(cfg.id)
        , is_voter(cfg.is_voter)
        , recent_recv(false)
    {}

    void mark_recent_recv() { recent_recv = true; }

    bool reset_recent_recv()
    {
        const bool old_recent_recv = recent_recv;
        recent_recv = false;
        return old_recent_recv;
    }

    const server_id_t id;
    bool is_voter;

    bool recent_recv;
};

} // namespace details
} // namespace raft
} // namespace wstux

#endif /* _LIBS_RAFT_CONNECTION_PEER_H_ */
