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

#ifndef _LIBS_RAFT_LOGGER_H_
#define _LIBS_RAFT_LOGGER_H_

#include <cstdarg>
#include <cstdio>

#include "raft/io.h"

namespace wstux {
namespace raft {
namespace details {

struct logger
{
    explicit logger(logging_handler::ptr p_handler)
        : p_hdlr(std::move(p_handler))
    {}

    bool cal_log(logging_handler::severity_level lvl) const { return p_hdlr && p_hdlr->can_log_fn(p_hdlr->p_this, lvl); }

    bool can_root_log(logging_handler::severity_level lvl) const { return cal_log(lvl); }
    bool can_heartbeat_log(logging_handler::severity_level lvl) const { return is_heartbeat_channel_enabled && cal_log(lvl); }
    bool can_timeout_log(logging_handler::severity_level lvl) const { return is_timeout_channel_enabled && cal_log(lvl); }
    bool can_vote_log(logging_handler::severity_level lvl) const { return is_vote_channel_enabled && cal_log(lvl); }

    void log(logging_handler::severity_level lvl, const char* p_fmt, ...)
    {
        char buffer[256];

        va_list args;
        va_start(args, p_fmt);
        std::vsnprintf(buffer, sizeof(buffer), p_fmt, args);
        va_end(args);

        p_hdlr->log_fn(p_hdlr->p_this, lvl, buffer);
    }

    bool is_heartbeat_channel_enabled = true;
    bool is_timeout_channel_enabled = true;
    bool is_vote_channel_enabled = true;
    logging_handler::ptr p_hdlr;
};

} // namespace details
} // namespace raft
} // namespace wstux

#define _RAFT_LOG(logger, channel, level, fmt, ...)                         \
    do {                                                                    \
        if (logger.can_## channel ##_log(level)) {                          \
            logger.log(level, fmt, ##__VA_ARGS__);                          \
        }                                                                   \
    } while(0)

#define _RAFT_CH_LOG(logger, channel, channel_name, level, fmt, ...)        \
    do {                                                                    \
        if (logger.can_## channel ##_log(level)) {                          \
            logger.log(level, fmt, ##__VA_ARGS__);                          \
        }                                                                   \
    } while(0)

#define _RAFT_LOG_EMERG_LVL  ::wstux::raft::logging_handler::severity_level::emerg
#define _RAFT_LOG_FATAL_LVL  ::wstux::raft::logging_handler::severity_level::fatal
#define _RAFT_LOG_CRIT_LVL   ::wstux::raft::logging_handler::severity_level::crit
#define _RAFT_LOG_ERROR_LVL  ::wstux::raft::logging_handler::severity_level::error
#define _RAFT_LOG_WARN_LVL   ::wstux::raft::logging_handler::severity_level::warning
#define _RAFT_LOG_NOTICE_LVL ::wstux::raft::logging_handler::severity_level::notice
#define _RAFT_LOG_INFO_LVL   ::wstux::raft::logging_handler::severity_level::info
#define _RAFT_LOG_DEBUG_LVL  ::wstux::raft::logging_handler::severity_level::debug
#define _RAFT_LOG_TRACE_LVL  ::wstux::raft::logging_handler::severity_level::trace

#define RAFT_LOG_EMERG(ctx,  fmt, ...)  _RAFT_LOG(ctx.raft_logger, root, _RAFT_LOG_EMERG_LVL,  fmt, ##__VA_ARGS__)
#define RAFT_LOG_FATAL(ctx,  fmt, ...)  _RAFT_LOG(ctx.raft_logger, root, _RAFT_LOG_FATAL_LVL,  fmt, ##__VA_ARGS__)
#define RAFT_LOG_CRIT(ctx,   fmt, ...)  _RAFT_LOG(ctx.raft_logger, root, _RAFT_LOG_CRIT_LVL,   fmt, ##__VA_ARGS__)
#define RAFT_LOG_ERROR(ctx,  fmt, ...)  _RAFT_LOG(ctx.raft_logger, root, _RAFT_LOG_ERROR_LVL,  fmt, ##__VA_ARGS__)
#define RAFT_LOG_WARN(ctx,   fmt, ...)  _RAFT_LOG(ctx.raft_logger, root, _RAFT_LOG_WARN_LVL,   fmt, ##__VA_ARGS__)
#define RAFT_LOG_NOTICE(ctx, fmt, ...)  _RAFT_LOG(ctx.raft_logger, root, _RAFT_LOG_NOTICE_LVL, fmt, ##__VA_ARGS__)
#define RAFT_LOG_INFO(ctx,   fmt, ...)  _RAFT_LOG(ctx.raft_logger, root, _RAFT_LOG_INFO_LVL,   fmt, ##__VA_ARGS__)
#define RAFT_LOG_DEBUG(ctx,  fmt, ...)  _RAFT_LOG(ctx.raft_logger, root, _RAFT_LOG_DEBUG_LVL,  fmt, ##__VA_ARGS__)
#define RAFT_LOG_TRACE(ctx,  fmt, ...)  _RAFT_LOG(ctx.raft_logger, root, _RAFT_LOG_TRACE_LVL,  fmt, ##__VA_ARGS__)

#define RAFT_LOG_CH_EMERG(ctx,  channel, channel_name, fmt, ...)            \
    _RAFT_CH_LOG(ctx.raft_logger, channel, channel_name, _RAFT_LOG_EMERG_LVL,  fmt, ##__VA_ARGS__)
#define RAFT_LOG_CH_FATAL(ctx,  channel, channel_name, fmt, ...)            \
    _RAFT_CH_LOG(ctx.raft_logger, channel, channel_name, _RAFT_LOG_FATAL_LVL,  fmt, ##__VA_ARGS__)
#define RAFT_LOG_CH_CRIT(ctx,   channel, channel_name, fmt, ...)            \
    _RAFT_CH_LOG(ctx.raft_logger, channel, channel_name, _RAFT_LOG_CRIT_LVL,   fmt, ##__VA_ARGS__)
#define RAFT_LOG_CH_ERROR(ctx,  channel, channel_name, fmt, ...)            \
    _RAFT_CH_LOG(ctx.raft_logger, channel, channel_name, _RAFT_LOG_ERROR_LVL,  fmt, ##__VA_ARGS__)
#define RAFT_LOG_CH_WARN(ctx,   channel, channel_name, fmt, ...)            \
    _RAFT_CH_LOG(ctx.raft_logger, channel, channel_name, _RAFT_LOG_WARN_LVL,   fmt, ##__VA_ARGS__)
#define RAFT_LOG_CH_NOTICE(ctx, channel, channel_name, fmt, ...)            \
    _RAFT_CH_LOG(ctx.raft_logger, channel, channel_name, _RAFT_LOG_NOTICE_LVL, fmt, ##__VA_ARGS__)
#define RAFT_LOG_CH_INFO(ctx,   channel, channel_name, fmt, ...)            \
    _RAFT_CH_LOG(ctx.raft_logger, channel, channel_name, _RAFT_LOG_INFO_LVL,   fmt, ##__VA_ARGS__)
#define RAFT_LOG_CH_DEBUG(ctx,  channel, channel_name, fmt, ...)            \
    _RAFT_CH_LOG(ctx.raft_logger, channel, channel_name, _RAFT_LOG_DEBUG_LVL,  fmt, ##__VA_ARGS__)
#define RAFT_LOG_CH_TRACE(ctx,  channel, channel_name, fmt, ...)            \
    _RAFT_CH_LOG(ctx.raft_logger, channel, channel_name, _RAFT_LOG_TRACE_LVL,  fmt, ##__VA_ARGS__)

#define RAFT_HB_LOG_EMERG(ctx,  fmt, ...)  RAFT_LOG_CH_EMERG(ctx,  heartbeat, "Heartbeat", fmt, ##__VA_ARGS__)
#define RAFT_HB_LOG_FATAL(ctx,  fmt, ...)  RAFT_LOG_CH_FATAL(ctx,  heartbeat, "Heartbeat", fmt, ##__VA_ARGS__)
#define RAFT_HB_LOG_CRIT(ctx,   fmt, ...)  RAFT_LOG_CH_CRIT(ctx,   heartbeat, "Heartbeat", fmt, ##__VA_ARGS__)
#define RAFT_HB_LOG_ERROR(ctx,  fmt, ...)  RAFT_LOG_CH_ERROR(ctx,  heartbeat, "Heartbeat", fmt, ##__VA_ARGS__)
#define RAFT_HB_LOG_WARN(ctx,   fmt, ...)  RAFT_LOG_CH_WARN(ctx,   heartbeat, "Heartbeat", fmt, ##__VA_ARGS__)
#define RAFT_HB_LOG_NOTICE(ctx, fmt, ...)  RAFT_LOG_CH_NOTICE(ctx, heartbeat, "Heartbeat", fmt, ##__VA_ARGS__)
#define RAFT_HB_LOG_INFO(ctx,   fmt, ...)  RAFT_LOG_CH_INFO(ctx,   heartbeat, "Heartbeat", fmt, ##__VA_ARGS__)
#define RAFT_HB_LOG_DEBUG(ctx,  fmt, ...)  RAFT_LOG_CH_DEBUG(ctx,  heartbeat, "Heartbeat", fmt, ##__VA_ARGS__)
#define RAFT_HB_LOG_TRACE(ctx,  fmt, ...)  RAFT_LOG_CH_TRACE(ctx,  heartbeat, "Heartbeat", fmt, ##__VA_ARGS__)

#define RAFT_TO_LOG_EMERG(ctx,  fmt, ...)  RAFT_LOG_CH_EMERG(ctx,  timeout, "Timeout", fmt, ##__VA_ARGS__)
#define RAFT_TO_LOG_FATAL(ctx,  fmt, ...)  RAFT_LOG_CH_FATAL(ctx,  timeout, "Timeout", fmt, ##__VA_ARGS__)
#define RAFT_TO_LOG_CRIT(ctx,   fmt, ...)  RAFT_LOG_CH_CRIT(ctx,   timeout, "Timeout", fmt, ##__VA_ARGS__)
#define RAFT_TO_LOG_ERROR(ctx,  fmt, ...)  RAFT_LOG_CH_ERROR(ctx,  timeout, "Timeout", fmt, ##__VA_ARGS__)
#define RAFT_TO_LOG_WARN(ctx,   fmt, ...)  RAFT_LOG_CH_WARN(ctx,   timeout, "Timeout", fmt, ##__VA_ARGS__)
#define RAFT_TO_LOG_NOTICE(ctx, fmt, ...)  RAFT_LOG_CH_NOTICE(ctx, timeout, "Timeout", fmt, ##__VA_ARGS__)
#define RAFT_TO_LOG_INFO(ctx,   fmt, ...)  RAFT_LOG_CH_INFO(ctx,   timeout, "Timeout", fmt, ##__VA_ARGS__)
#define RAFT_TO_LOG_DEBUG(ctx,  fmt, ...)  RAFT_LOG_CH_DEBUG(ctx,  timeout, "Timeout", fmt, ##__VA_ARGS__)
#define RAFT_TO_LOG_TRACE(ctx,  fmt, ...)  RAFT_LOG_CH_TRACE(ctx,  timeout, "Timeout", fmt, ##__VA_ARGS__)

#define RAFT_VOTE_LOG_EMERG(ctx,  fmt, ...)  RAFT_LOG_CH_EMERG(ctx,  vote, "Vote", fmt, ##__VA_ARGS__)
#define RAFT_VOTE_LOG_FATAL(ctx,  fmt, ...)  RAFT_LOG_CH_FATAL(ctx,  vote, "Vote", fmt, ##__VA_ARGS__)
#define RAFT_VOTE_LOG_CRIT(ctx,   fmt, ...)  RAFT_LOG_CH_CRIT(ctx,   vote, "Vote", fmt, ##__VA_ARGS__)
#define RAFT_VOTE_LOG_ERROR(ctx,  fmt, ...)  RAFT_LOG_CH_ERROR(ctx,  vote, "Vote", fmt, ##__VA_ARGS__)
#define RAFT_VOTE_LOG_WARN(ctx,   fmt, ...)  RAFT_LOG_CH_WARN(ctx,   vote, "Vote", fmt, ##__VA_ARGS__)
#define RAFT_VOTE_LOG_NOTICE(ctx, fmt, ...)  RAFT_LOG_CH_NOTICE(ctx, vote, "Vote", fmt, ##__VA_ARGS__)
#define RAFT_VOTE_LOG_INFO(ctx,   fmt, ...)  RAFT_LOG_CH_INFO(ctx,   vote, "Vote", fmt, ##__VA_ARGS__)
#define RAFT_VOTE_LOG_DEBUG(ctx,  fmt, ...)  RAFT_LOG_CH_DEBUG(ctx,  vote, "Vote", fmt, ##__VA_ARGS__)
#define RAFT_VOTE_LOG_TRACE(ctx,  fmt, ...)  RAFT_LOG_CH_TRACE(ctx,  vote, "Vote", fmt, ##__VA_ARGS__)

#endif /* _LIBS_RAFT_LOGGER_H_ */
