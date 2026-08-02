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

#ifndef _EXAMPLES_RAFT_LEADER_ELECTION_COUNTER_LOGGING_H_
#define _EXAMPLES_RAFT_LEADER_ELECTION_COUNTER_LOGGING_H_

#include <sys/time.h>

#include <iostream>
#include <mutex>
#include <sstream>
#include <thread>

#include "raft_le/io.h"

namespace wstux {
namespace examples {
namespace counter {
namespace details {

enum log_level : int
{
    error = 0,
    warning = 1,
    info = 2,
    debug = 3,
    trace = 4
};

class logger : public raft::le::logger
{
public:
    logger(log_level lvl, const std::string& ch)
        : raft::le::logger()
    {
        can_error_log_fn   = std::bind(&logger::can_log, lvl, log_level::error);
        log_error_fn       = std::bind(&logger::log, "ERROR", ch, std::placeholders::_1);
        can_warning_log_fn = std::bind(&logger::can_log, lvl, log_level::warning);
        log_warning_fn     = std::bind(&logger::log, "WARN ", ch, std::placeholders::_1);
        can_info_log_fn    = std::bind(&logger::can_log, lvl, log_level::info);
        log_info_fn        = std::bind(&logger::log, "INFO ", ch, std::placeholders::_1);
        can_debug_log_fn   = std::bind(&logger::can_log, lvl, log_level::debug);
        log_debug_fn       = std::bind(&logger::log, "DEBUG", ch, std::placeholders::_1);
        can_trace_log_fn   = std::bind(&logger::can_log, lvl, log_level::trace);
        log_trace_fn       = std::bind(&logger::log, "TRACE", ch, std::placeholders::_1);
    }

private:
    static bool can_log(log_level severity_level, log_level lvl)
    {
        return (lvl <= severity_level);
    }

    static int log(const std::string& lvl, std::string ch, const std::string& msg)
    {
        static std::mutex cout_mutex;

        const std::string ts = timestamp();
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cout << ts << " <" << std::this_thread::get_id() << "> [" << lvl << "] <" << ch << "> " << msg;
        return msg.size();
    }

    static std::string timestamp()
    {
        constexpr size_t ts_size = 24;
        char buf[ts_size];

        struct timeval cur_tv;
        struct tm cur_tm;
        if (gettimeofday(&cur_tv, NULL) != 0 || localtime_r(&cur_tv.tv_sec, &cur_tm) == NULL) {
            return "";
        }
        int rc = snprintf(buf, ts_size, "%04d-%02d-%02d %02d:%02d:%02d.%03d",
                    cur_tm.tm_year + 1900, cur_tm.tm_mon + 1, cur_tm.tm_mday,
                    cur_tm.tm_hour, cur_tm.tm_min, cur_tm.tm_sec, (int)(cur_tv.tv_usec / 1000));
        if (rc < 0) {
            return "";
        }
        buf[rc] = '\0';
        return std::string(buf, ts_size - 1);
    }
};

class logger_factory : public raft::le::ilogger_factory
{
public:
    explicit logger_factory(log_level lvl)
        : m_level(lvl)
    {}

    virtual raft::le::logger get_logger(const std::string& ch) override { return logger(m_level, ch); }

private:
    log_level m_level;
};

} // namespace details
} // namespace counter
} // namespace examples
} // namespace wstux

#define _COUNTER_LOG(logger, level, VARS)                                   \
    do {                                                                    \
        if (! logger.can_## level ##_log_fn()) {                            \
            break;                                                          \
        }                                                                   \
        std::stringstream ss;                                               \
        ss << VARS << std::endl;                                            \
        logger.log_## level ##_fn(ss.str());                                \
    }                                                                       \
    while (0)

#define LOG_ERROR(logger, VARS)    _COUNTER_LOG(logger, error,   VARS)
#define LOG_WARN(logger,  VARS)    _COUNTER_LOG(logger, warning, VARS)
#define LOG_INFO(logger,  VARS)    _COUNTER_LOG(logger, info,    VARS)
#define LOG_DEBUG(logger, VARS)    _COUNTER_LOG(logger, debug,   VARS)
#define LOG_TRACE(logger, VARS)    _COUNTER_LOG(logger, trace,   VARS)

#endif /* _EXAMPLES_RAFT_COUNTER_LOGGING_H_ */
