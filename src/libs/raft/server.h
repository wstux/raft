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

#ifndef _LIBS_RAFT_SERVER_H_
#define _LIBS_RAFT_SERVER_H_

#include <atomic>
#include <memory>
#include <string>
#include <vector>

#include "raft/io.h"

namespace wstux {
namespace raft {
namespace details { struct context; }

class server final
{
public:
    using allocator_type = std::allocator<server>;
    using ptr = std::shared_ptr<server>;

public:
    server(const server_id_t id, const io::ptr& p_io, const fsm::ptr p_fsm, logging_handler::ptr p_handler,
           const is_stop_fn_t& is_stop_fn, const allocator_type& alloc = allocator_type());

    ~server();

    void add(const server_id_t id, const std::string& address, const bool is_voter);

    void deinit();

    server_id_t id() const { return m_id; }

    bool init();

    bool is_inited() const;

    bool is_leader() const;

    bool is_stop() const { return m_is_stop || m_is_stop_fn(); }

    void handle_message(const inbuffer_type& msg_buf);

    bool reconfigure();

    void remove(const server_id_t id);

    bool start();

    void stop();

private:
    using context_ptr = std::shared_ptr<details::context>;

private:
    static bool load(details::context& ctx);

private:
    const server_id_t m_id;
    allocator_type m_alloc;

    is_stop_fn_t m_is_stop_fn;
    std::atomic_bool m_is_stop;

    context_ptr m_p_ctx;
};

} // namespace raft
} // namespace wstux

#endif /* _LIBS_RAFT_SERVER_H_ */
