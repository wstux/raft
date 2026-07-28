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

#ifndef _TESTS_RAFT_LEADER_ELECTION_IO_STUB_H_
#define _TESTS_RAFT_LEADER_ELECTION_IO_STUB_H_

#include <sys/time.h>

#include <chrono>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <random>
#include <thread>

#include <boost/lockfree/queue.hpp>

#include "raft_le/io.h"
#include "raft_le/server.h"

namespace wstux {
namespace raft {
namespace le {
namespace tests {

class logger_factory : public ilogger_factory
{
private:
    struct logger_data final
    {
        using ptr = std::shared_ptr<logger_data>;

        explicit logger_data(const std::string& file_path)
            : enable_console_log(file_path.empty() || false)
            , line_number(0)
            , fout(file_path, std::ios::out | std::ios::app)
        {
            if (logger_factory::is_enable_logging && ! fout.is_open()) {
                std::cerr << "Failed to open log file " << file_path << std::endl;
            }
        }

        const bool enable_console_log;
        size_t line_number;
        std::ofstream fout;
        std::mutex mutex;
    };

    class logger : public ::wstux::raft::le::logger
    {
    public:
        logger(const std::string& ch, logger_data::ptr p_data)
            : raft::le::logger()
        {
            const bool is_enable_logging = logger_factory::is_enable_logging;
            can_error_log_fn   = [is_enable_logging]() -> bool { return is_enable_logging; };
            log_error_fn       = [ch, p_data](const std::string& msg) { return log(p_data, "ERROR", ch, msg); };
            can_warning_log_fn = [is_enable_logging]() -> bool { return is_enable_logging; };
            log_warning_fn     = [ch, p_data](const std::string& msg) { return log(p_data, "WARN", ch, msg); };
            can_info_log_fn    = [is_enable_logging]() -> bool { return is_enable_logging; };
            log_info_fn        = [ch, p_data](const std::string& msg) { return log(p_data, "INFO", ch, msg); };
            can_debug_log_fn   = [is_enable_logging]() -> bool { return is_enable_logging; };
            log_debug_fn       = [ch, p_data](const std::string& msg) { return log(p_data, "DEBUG", ch, msg); };
            can_trace_log_fn   = [is_enable_logging]() -> bool { return is_enable_logging; };
            log_trace_fn       = [ch, p_data](const std::string& msg) { return log(p_data, "TRACE", ch, msg); };
        }

    private:
        template<typename TOutStream>
        static int log_msg(TOutStream& sout, std::string lvl, std::string ch, const std::string& msg, const size_t* p_line_number = nullptr)
        {
            if (p_line_number != nullptr) {
                sout << (*p_line_number) << " ";
            }
            sout << timestamp() << " <" << std::this_thread::get_id() << "> [" << lvl << "] <" << ch << "> " << msg;
            return msg.size();
        }

        static int log(std::string lvl, std::string ch, const std::string& msg)
        {
            static std::mutex mutex;
            std::lock_guard<std::mutex> lock(mutex);

            return log_msg(std::cout, lvl, ch, msg);
            //std::cout << timestamp() << " <" << std::this_thread::get_id() << "> [" << lvl << "] <" << ch << "> " << msg;
            //return msg.size();
        }

        static int log(logger_data::ptr p_data, std::string lvl, std::string ch, const std::string& msg)
        {
            if (! p_data) {
                return 0;
            }
            if (p_data->fout.is_open()) {
                std::lock_guard<std::mutex> lock(p_data->mutex);
                log_msg(p_data->fout, lvl, ch, msg, &(++p_data->line_number));
                p_data->fout.flush();
            }
            if (p_data->enable_console_log) {
                log_msg(std::cout, lvl, ch, msg);
            }
            return msg.size();
        }

        static int timestamp(char* buf, size_t size)
        {
            struct timeval cur_tv;
            struct tm cur_tm;

            if (gettimeofday(&cur_tv, NULL) != 0) {
                return -1;
            }
            if (localtime_r(&cur_tv.tv_sec, &cur_tm) == NULL) {
                return -1;
            }

            int rc = snprintf(buf, size, "%04d-%02d-%02d %02d:%02d:%02d.%03d",
                        cur_tm.tm_year + 1900, cur_tm.tm_mon + 1, cur_tm.tm_mday,
                        cur_tm.tm_hour, cur_tm.tm_min, cur_tm.tm_sec, (int)(cur_tv.tv_usec / 1000));
            if (rc < 0) {
                return -1;
            }
            buf[rc] = '\0';
            return 0;
        }

        static std::string timestamp()
        {
            constexpr size_t ts_size = 24;
            char cur_ts[ts_size];
            if (timestamp(cur_ts, ts_size) != 0) {
                return "";
            }

            return std::string(cur_ts, ts_size - 1);
        }
    };

public:
    explicit logger_factory(const std::string& file_path = std::string())
        : m_p_data(std::make_shared<logger_data>(file_path))
    {}

    virtual raft::le::logger get_logger(const std::string& ch) override { return logger_factory::logger(ch, m_p_data); }

public:
    static const bool is_enable_logging;

private:
    logger_data::ptr m_p_data;
};

const bool logger_factory::is_enable_logging = false;

enum client_type
{
    single,
    threaded,
    random_threaded
};

class client_stub final : public iclient
{
public:
    using ptr = std::shared_ptr<client_stub>;

public:
    explicit client_stub(std::shared_ptr<server> p_srv)
        : m_id(p_srv->id())
        , m_p_srv(p_srv)
    {}

    virtual ~client_stub() {}

    virtual void send(const buffer_type& msg) override { m_p_srv->handle_message(msg); }

private:
    const int32_t m_id;
    std::shared_ptr<server> m_p_srv;
};

class threaded_client_stub final : public iclient
{
public:
    using ptr = std::shared_ptr<client_stub>;

    struct queue_data
    {
        buffer_type msg;
    };

public:
    threaded_client_stub(std::shared_ptr<server> p_srv, client_type type, std::function<bool()> is_stop)
        : m_id(p_srv->id())
        , m_is_stop_fn(is_stop)
        , m_p_srv(p_srv)
        , m_type(type)
        , m_rand_engine(std::chrono::system_clock::now().time_since_epoch().count())
        , m_sleep_for(15, 50)
        , m_queue(128)
    {
        const size_t threads_count = 2;
        m_threads_pool.reserve(threads_count);
        for (size_t i = 0; i < threads_count; ++i) {
            m_threads_pool.emplace_back(std::make_shared<std::thread>([this]() { thread_main(); }));
        }
    }

    virtual ~threaded_client_stub()
    {
        m_is_stop = true;
        for (std::shared_ptr<std::thread>& p_thread : m_threads_pool) {
            p_thread->join();
        }

        queue_data* d;
        while (m_queue.pop(d)) {
            delete d;
        }
    }

    virtual void send(const buffer_type& msg) override
    {
        queue_data* d = new queue_data();
        d->msg = msg;

        while (! m_queue.push(d)) {}
    }

private:
    std::chrono::duration<size_t, std::milli> sleep_for()
    {
        using namespace std::chrono_literals;

        if (m_type == client_type::random_threaded) {
            const size_t sleep_for_ms = m_sleep_for(m_rand_engine);
            return std::chrono::milliseconds(sleep_for_ms);
        }
        return 25ms;
    }

    void thread_main()
    {
        while(! m_is_stop && ! m_is_stop_fn()) {
            queue_data* d;
            while (m_queue.pop(d)) {
                m_p_srv->handle_message(d->msg);
                delete d;
            }
            std::this_thread::sleep_for(sleep_for());
        }
    }

private:
    const int32_t m_id;
    std::atomic_bool m_is_stop{false};
    std::function<bool()> m_is_stop_fn;
    std::shared_ptr<server> m_p_srv;
    client_type m_type;

    std::mt19937 m_rand_engine;
    std::uniform_int_distribution<size_t> m_sleep_for;
    boost::lockfree::queue<queue_data*> m_queue;
    std::vector<std::shared_ptr<std::thread>> m_threads_pool;
};

class io_stub final : public io
{
public:
    using ptr = std::shared_ptr<io_stub>;

    class iclient_factory
    {
    public:
        using ptr = std::shared_ptr<iclient_factory>;

    public:
        virtual ~iclient_factory() {}

        virtual iclient::ptr create_client(server_id_t id, const std::string& endpoint) const = 0;
    };

    class empty_clients_factory : public tests::io_stub::iclient_factory
    {
    public:
        class empty_client_stub final : public iclient
        {
        public:
            virtual ~empty_client_stub() {}
            virtual void send(const buffer_type&) override {}
        };

    public:
        virtual ~empty_clients_factory() {}

        virtual iclient::ptr create_client(server_id_t, const std::string&) const { return std::make_shared<empty_client_stub>();}
    };

    struct results final
    {
        using ptr = std::shared_ptr<results>;

        bool init = true;
        bool load = true;

        config cfg;

        term_t term = 0;
    };

public:
    io_stub(const std::shared_ptr<cluster_config>& p_cluster_cfg, const iclient_factory::ptr& p_factory, results::ptr p_results = nullptr)
        : m_p_cluster_cfg(p_cluster_cfg)
        , m_p_factory(p_factory)
        , m_p_results(p_results)
        , m_term(0)
        , m_voted_for(gk_invalid_id)
    {
        if (m_p_results) {
            m_cfg = m_p_results->cfg;
            m_term = m_p_results->term;
        } else {
            m_p_results = std::make_shared<results>();
        }
        m_cfg.scheduler_threads_count = 2;
    }

    virtual ~io_stub() {}

    virtual const cluster_config& bootstrap() const override final { return *m_p_cluster_cfg; }

    virtual const config& configuration() const override final { return m_cfg; };

    virtual iclient::ptr create_client(server_id_t id, const std::string& endpoint) const override final
    {
        return m_p_factory->create_client(id, endpoint);
    }

    virtual void deinit() override final {}

    virtual bool init(server_id_t) override final { return m_p_results->init; }

    virtual bool load() override final { return m_p_results->load; }

    virtual term_t load_term() override final { return m_term; }

    virtual void set_term(term_t term) override final { m_term = term; }

    virtual void set_voted_for(server_id_t id) override final { m_voted_for = id; }

    virtual server_id_t voted_for() const override final { return m_voted_for; }

private:
    std::shared_ptr<cluster_config> m_p_cluster_cfg;
    iclient_factory::ptr m_p_factory;
    config m_cfg;
    results::ptr m_p_results;

    term_t m_term;
    server_id_t m_voted_for;
};

} // namespace tests
} // namespace le
} // namespace raft
} // namespace wstux

#endif /* _TESTS_RAFT_LEADER_ELECTION_IO_STUB_H_ */
