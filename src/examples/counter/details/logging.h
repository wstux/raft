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

class logging_handler final : public raft::le::logging_handler
{
public:
    explicit logging_handler(severity_level lvl)
        : raft::le::logging_handler()
        , m_severity_level(lvl)
    {
        p_this = &m_severity_level;
        can_log_fn = &can_log;
        log_fn = &log;
    }

    static bool can_log(void* p_this, severity_level lvl)
    {
        if (p_this == nullptr) {
            return false;
        }
        const severity_level& severity_lvl = *(static_cast<const severity_level*>(p_this));
        return (lvl <= severity_lvl);
    }

    static void log(void*, const severity_level lvl, const char* p_msg)
    {
        static std::mutex cout_mutex;

        const std::string ts = timestamp();
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cout << ts << " <" << std::this_thread::get_id() << "> [" << lvl << "] <Raft> " << p_msg << std::endl;
    }

private:
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

private:
    raft::le::logging_handler::severity_level m_severity_level;
};

} // namespace details
} // namespace counter
} // namespace examples
} // namespace wstux

#define _COUNTER_LOG(logger, level, VARS)                                   \
    do {                                                                    \
        if (logger.can_log_fn(logger.p_this, level)) {                      \
            std::stringstream ss;                                           \
            ss << VARS;                                                     \
            logger.log_fn(logger.p_this, level, ss.str().c_str());          \
        }                                                                   \
    } while(0)

#define LOG_ERROR(logger, VARS)    _COUNTER_LOG(logger, ::wstux::raft::le::logging_handler::severity_level::error,   VARS)
#define LOG_WARN(logger,  VARS)    _COUNTER_LOG(logger, ::wstux::raft::le::logging_handler::severity_level::warning, VARS)
#define LOG_INFO(logger,  VARS)    _COUNTER_LOG(logger, ::wstux::raft::le::logging_handler::severity_level::info,    VARS)
#define LOG_DEBUG(logger, VARS)    _COUNTER_LOG(logger, ::wstux::raft::le::logging_handler::severity_level::debug,   VARS)
#define LOG_TRACE(logger, VARS)    _COUNTER_LOG(logger, ::wstux::raft::le::logging_handler::severity_level::trace,   VARS)

#endif /* _EXAMPLES_RAFT_COUNTER_LOGGING_H_ */
