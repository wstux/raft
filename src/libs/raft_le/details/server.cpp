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

#include <cassert>

#include "raft_le/server.h"
#include "raft_le/details/logger_channels.h"

namespace wstux {
namespace raft {
namespace le {

////////////////////////////////////////////////////////////////////////////////
// class server

server::server(const server_id_t id, const io::ptr& /*p_io*/, const ilogger_factory::ptr /*p_factory*/, const is_stop_fn_t& is_stop_fn)
    : m_id(id)
    , m_is_stop_fn(is_stop_fn)
    , m_is_stop(true)
{
    assert(m_id != gk_invalid_id);
}

cluster_config_t server::cluster_cfg() const
{
    return {};
}

void server::deinit()
{}

const std::string& server::endpoint() const
{
    static std::string ep;
    return ep;
}

bool server::init()
{
    return false;
}

bool server::is_inited() const
{
    return false;
}

bool server::is_leader() const
{
    return false;
}

void server::handle_message(const buffer_type& /*msg_buf*/)
{
    if (is_stop()) {
        return;
    }
}

std::vector<std::string> server::logging_channels()
{
    return details::loggers::logging_channels();
}

bool server::start()
{
    if (! m_is_stop.exchange(false)) {
        return false;
    }

    if (! is_inited()) {
        return false;
    }

    return true;
}

void server::stop()
{}

} // namespace le
} // namespace raft
} // namespace wstux
