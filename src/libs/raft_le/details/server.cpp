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
#include "raft_le/details/connection/messages.h"
#include "raft_le/details/connection/serialization.h"
#include "raft_le/details/handlers/heartbeat_handler.h"
#include "raft_le/details/handlers/timeout_handler.h"
#include "raft_le/details/handlers/vote_handler.h"
#include "raft_le/details/role/convert.h"
#include "raft_le/details/role/election.h"

namespace wstux {
namespace raft {
namespace le {
namespace {

void handle_message(details::context& ctx, const details::message& msg)
{
    std::unique_lock<std::mutex> lock(ctx.handler_mutex);

    switch(msg.type) {
    case details::message_type::heartbeat_request:
        details::heartbeat::handle_request(ctx, msg.term, msg.src_id, msg.heartbeat_req);
        break;
    case details::message_type::heartbeat_response:
        details::heartbeat::handle_response(ctx, msg.term, msg.src_id, msg.heartbeat_resp);
        break;
    case details::message_type::vote_request:
        details::vote::handle_request(ctx, msg.term, msg.src_id, msg.vote_req);
        break;
    case details::message_type::vote_response:
        details::vote::handle_response(ctx, msg.term, msg.src_id, msg.vote_resp);
        break;
    default:
        RAFT_ROOT_LOG_WARN(ctx, "Unsupported message type " << msg.type);
        break;
    }
}

} // <anonymous> namespace

////////////////////////////////////////////////////////////////////////////////
// class server

server::server(const server_id_t id, const io::ptr& p_io, const ilogger_factory::ptr p_factory, const is_stop_fn_t& is_stop_fn)
    : m_id(id)
    , m_is_stop_fn(is_stop_fn)
    , m_is_stop(true)
    , m_p_ctx(std::make_unique<details::context>(id, p_io, p_factory, is_stop_fn))
{
    static_assert(std::is_same<context_ptr, details::context::ptr>::value, "Invalid context pointer type");

    assert(m_id != gk_invalid_id);
    details::role::become_follower(*m_p_ctx);
}

void server::deinit()
{
    m_p_ctx->p_io->deinit();
}

const std::string& server::endpoint() const
{
    return m_p_ctx->config.endpoint;
}

bool server::init()
{
    const bool is_inited = details::utils::init(*m_p_ctx, m_id);
    if (! is_inited) {
        RAFT_ROOT_LOG_ERROR((*m_p_ctx), "Filed to init raft server.");
        return false;
    }
    m_p_ctx->election_task = m_p_ctx->p_scheduler->make_task(std::bind(&details::timeout::election_timeout_task, std::ref(*m_p_ctx)));
    m_p_ctx->heartbeat_task = m_p_ctx->p_scheduler->make_task(std::bind(&details::timeout::heartbeat_timeout_task, std::ref(*m_p_ctx)));

    return true;
}

bool server::is_inited() const
{
    return (m_p_ctx->election_task.get() != nullptr);
}

bool server::is_leader() const
{
    return m_p_ctx->role.is_leader();
}

void server::handle_message(const buffer_type& msg_buf)
{
    if (is_stop()) {
        return;
    }

    details::message msg;
    details::deserialize(msg_buf, msg);

    le::handle_message(*m_p_ctx, msg);
}

bool server::load(details::context& ctx)
{
    const bool is_loaded = details::utils::load(ctx);
    if (! is_loaded) {
        RAFT_ROOT_LOG_ERROR(ctx, "Failed to load raft configuration.");
        return false;
    }

    details::role::become_follower(ctx);
    return true;
}

std::vector<std::string> server::logging_channels()
{
    return details::loggers::logging_channels();
}

bool server::reconfigure()
{
    const config cfg = m_p_ctx->p_io->configuration();
    if (cfg.heartbeat_interval_ms == 0 || cfg.vote_timeout_max_ms == 0 || cfg.vote_timeout_max_ms < cfg.vote_timeout_min_ms) {
        return false;
    }

    const cluster_config cluster_cfg = m_p_ctx->p_io->bootstrap();

    m_p_ctx->p_scheduler->reconfigure(cfg.scheduler_threads_count);

    std::unique_lock<std::mutex> lock(m_p_ctx->handler_mutex);
    m_p_ctx->election_distribution = std::uniform_int_distribution<size_t>(cfg.vote_timeout_min_ms, cfg.vote_timeout_max_ms);

    m_p_ctx->heartbeat_interval_ms = cfg.heartbeat_interval_ms;
    m_p_ctx->heartbeat_probes_count = cfg.heartbeat_probes_count;

    details::peers::update(*m_p_ctx, cluster_cfg);
    return true;
}

bool server::start()
{
    std::unique_lock<std::mutex> lock(m_p_ctx->handler_mutex);
    if (! m_is_stop.exchange(false)) {
        RAFT_ROOT_LOG_WARN((*m_p_ctx), "Raft server " << m_p_ctx->id << " has been already started.");
        return false;
    }

    if (! is_inited()) {
        m_is_stop.exchange(true);
        return false;
    }

    if (! load(*m_p_ctx)) {
        m_is_stop.exchange(true);
        return false;
    }

    RAFT_ROOT_LOG_INFO((*m_p_ctx), "Starting raft server " << m_p_ctx->id << ".");
    m_p_ctx->p_scheduler->start();
    details::timeout::heartbeat_restart_task(*m_p_ctx);
    if (m_p_ctx->role.is_voter) {
        details::timeout::election_restart_task(*m_p_ctx);
    }

    details::role::initiate_election(*m_p_ctx);
    return true;
}

void server::stop()
{
    if (m_is_stop.exchange(true)) {
        RAFT_ROOT_LOG_WARN((*m_p_ctx), "Raft server " << m_p_ctx->id << " has been already stopped.");
        return;
    }
    RAFT_ROOT_LOG_INFO((*m_p_ctx), "Stopping raft server " << m_p_ctx->id << ".");

    m_p_ctx->p_scheduler->stop();
    details::timeout::election_cancel_task(*m_p_ctx);
    details::timeout::heartbeat_cancel_task(*m_p_ctx);
}

} // namespace le
} // namespace raft
} // namespace wstux
