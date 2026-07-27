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
#include "raft_le/details/context.h"
#include "raft_le/details/logger_channels.h"
#include "raft_le/details/logging.h"

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
    return details::utils::wrap(m_p_ctx, &details::utils::make_config);
}

void server::deinit()
{
    context_ptr p_ctx = m_p_ctx;
    p_ctx->p_io->deinit();
}

const std::string& server::endpoint() const
{
    context_ptr p_ctx = m_p_ctx;
    return p_ctx->config.endpoint;
}

bool server::init()
{
    context_ptr p_ctx = m_p_ctx;
    const bool is_inited = details::sturtup::init(*p_ctx, m_id);
    if (! is_inited) {
        RAFT_ROOT_LOG_ERROR((*p_ctx), "Filed to init raft server.");
        return false;
    }
    //m_p_ctx->election_task = ;
    //m_p_ctx->heartbeat_task = ;

    return true;
}

bool server::is_inited() const
{
    context_ptr p_ctx = m_p_ctx;
    return (p_ctx->election_task.get() != nullptr);
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

bool server::load(details::context& ctx)
{
    const bool is_loaded = details::sturtup::load(ctx);
    if (! is_loaded) {
        RAFT_ROOT_LOG_ERROR(ctx, "Failed to load raft configuration.");
        return false;
    }

    return true;
}

std::vector<std::string> server::logging_channels()
{
    return details::loggers::logging_channels();
}

bool server::start()
{
    context_ptr p_ctx = m_p_ctx;
    if (! m_is_stop.exchange(false)) {
        RAFT_ROOT_LOG_WARN((*p_ctx), "Raft server " << p_ctx->id << " has been already started.");
        return false;
    }

    if (! is_inited()) {
        return false;
    }

    if (! load(*p_ctx)) {
        return false;
    }

    RAFT_ROOT_LOG_INFO((*p_ctx), "Starting raft server " << p_ctx->id << ".");

    return true;
}

void server::stop()
{
    context_ptr p_ctx = m_p_ctx;
    if (m_is_stop.exchange(true)) {
        RAFT_ROOT_LOG_WARN((*p_ctx), "Raft server " << p_ctx->id << " has been already stopped.");
        return;
    }
    RAFT_ROOT_LOG_INFO((*p_ctx), "Stopping raft server " << p_ctx->id << ".");

    p_ctx->p_scheduler->stop();
}

} // namespace le
} // namespace raft
} // namespace wstux
