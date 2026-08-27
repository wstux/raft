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

#ifndef _TESTS_RAFT_LOGGING_HANDLER_STUB_H_
#define _TESTS_RAFT_LOGGING_HANDLER_STUB_H_

#include <sys/time.h>

#include <algorithm>
#include <chrono>
#include <fstream>
#include <filesystem>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <random>
#include <thread>

#include <boost/lockfree/queue.hpp>

#include "raft/io.h"

namespace wstux {
namespace raft {
namespace tests {

struct logging_handler_file : public logging_handler
{
    static constexpr bool is_enable_logging = false;

    struct logger_data final
    {
        using ptr = std::shared_ptr<logger_data>;

        explicit logger_data(const std::string& file_path)
            : enable_console_log(file_path.empty() || false)
            , line_number(0)
            , fout(file_path, std::ios::out | std::ios::app)
        {}

        const bool enable_console_log;
        size_t line_number;
        std::ofstream fout;
        std::mutex mutex;
    };

    explicit logging_handler_file(const std::string& file_path = std::string())
        : logging_handler()
        , p_data(std::make_shared<logger_data>(file_path))
    {
        p_this = p_data.get();
        can_log_fn = &can_log;
        log_fn = &log;
    }

    static bool can_log(void* p_this, severity_level) { return is_enable_logging && p_this != nullptr; }

    static void log(void* p_this, const severity_level lvl, const char* msg)
    {
        if (! p_this) {
            return;
        }
        logger_data* p_data = static_cast<logger_data*>(p_this);
        if (p_data->fout.is_open()) {
            std::lock_guard<std::mutex> lock(p_data->mutex);
            log_msg(p_data->fout, lvl, msg, &(++p_data->line_number));
        }
        if (p_data->enable_console_log) {
            static std::mutex mutex;
            std::lock_guard<std::mutex> lock(mutex);
            log_msg(std::cout, lvl, msg);
        }
    }

    template<typename TOutStream>
    static void log_msg(TOutStream& sout, logging_handler::severity_level lvl, const char* msg, const size_t* p_line_number = nullptr)
    {
        if (p_line_number != nullptr) {
            sout << (*p_line_number) << " ";
        }
        sout << timestamp() << " <" << std::this_thread::get_id() << "> [" << lvl << "] " << msg << std::endl;
        sout.flush();
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

    static std::string log_dir(const std::filesystem::path& nook_dir, const std::string& fixture, const std::string& name)
    {
        namespace fs = std::filesystem;

        if (! is_enable_logging) {
            return std::string();
        }

        if (! fs::exists(nook_dir.parent_path())) {
            return std::string();
        }

        fs::path logdir = nook_dir / "tests_log" / (fixture.empty() ? "" : fixture) / (name.empty() ? "" : name);
        if (! fs::exists(logdir)) {
            if (! fs::create_directories(logdir)) {
                return std::string();
            }
        }
        return logdir.string();
    }

    logger_data::ptr p_data;
};

} // namespace tests
} // namespace raft
} // namespace wstux

#endif /* _TESTS_RAFT_LOGGING_HANDLER_STUB_H_ */
