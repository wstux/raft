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

#include <algorithm>
#include <chrono>
#include <fstream>
#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <random>
#include <thread>
#include <unordered_map>

#include <boost/lockfree/queue.hpp>

#include "raft_le/io.h"
#include "raft_le/server.h"
#include "raft_le/details/connection/messages.h"
#include "raft_le/details/connection/serialization.h"

namespace wstux {
namespace raft {
namespace le {
namespace tests {

enum client_type
{
    single,
    threaded,
    random_threaded
};

class iclient
{
public:
    using ptr = std::shared_ptr<iclient>;

public:
    virtual ~iclient() {}
    virtual void send(const buffer_type& msg) = 0;
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

    virtual void send(const buffer_type& msg) override
    {
        if (std::shared_ptr<server> p_srv = m_p_srv.lock()) {
            p_srv->handle_message(msg);
        }
    }

private:
    const int32_t m_id;
    std::weak_ptr<server> m_p_srv;
};

class threaded_client_stub final : public iclient
{
public:
    using ptr = std::shared_ptr<client_stub>;

    struct queue_data
    {
        queue_data() : msg(buf.begin(), buf.end()) {}

        details::buffer_data_type buf;
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
        std::copy(msg.begin(), msg.end(), d->buf.begin());

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
        return 1ms;
    }

    void thread_main()
    {
        while(! m_is_stop && ! m_is_stop_fn()) {
            queue_data* d;
            while (m_queue.pop(d)) {
                //m_p_srv->handle_message(d->msg);
                std::shared_ptr<server> p_srv = m_p_srv.lock();
                if (p_srv) {
                    p_srv->handle_message(d->msg);
                }
                delete d;
            }
            std::this_thread::sleep_for(sleep_for());
        }
    }

private:
    const int32_t m_id;
    std::atomic_bool m_is_stop{false};
    std::function<bool()> m_is_stop_fn;
    std::weak_ptr<server> m_p_srv;
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

        virtual iclient::ptr create_client(server_id_t id) const = 0;
    };

public:
    io_stub(const std::shared_ptr<cluster_config>& p_cluster_cfg, const iclient_factory::ptr& p_factory)
        : m_p_cluster_cfg(p_cluster_cfg)
        , m_p_factory(p_factory)
        , m_term(0)
        , m_voted_for(gk_invalid_id)
    {
        m_cfg.scheduler_threads_count = 2;
    }

    virtual ~io_stub() {}

    virtual cluster_config bootstrap() const override final { return *m_p_cluster_cfg; }

    virtual config configuration() const override final { return m_cfg; };

    virtual void deinit() override final {}

    virtual bool init(server_id_t id) override final
    {
        if (m_clients.empty() && ! m_p_cluster_cfg->servers.empty()) {
            for (const server_config& cfg : m_p_cluster_cfg->servers) {
                if (cfg.id != id) {
                    m_clients.emplace(cfg.id, m_p_factory->create_client(cfg.id));
                }
            }
        }
        return true;
    }

    virtual term_t load_term() override final { return m_term; }

    virtual bool reconfigure(server_id_t) override final { return true; }

    virtual void send(server_id_t id, const buffer_type& msg) override final { m_clients.at(id)->send(msg); }

    virtual void set_term(term_t term) override final { m_term = term; }

    virtual void set_voted_for(server_id_t id) override final { m_voted_for = id; }

    virtual server_id_t voted_for() const override final { return m_voted_for; }

public:
    std::shared_ptr<cluster_config> m_p_cluster_cfg;
    std::unordered_map<server_id_t, iclient::ptr> m_clients;
    iclient_factory::ptr m_p_factory;
    config m_cfg;

    term_t m_term;
    server_id_t m_voted_for;
};

} // namespace tests
} // namespace le
} // namespace raft
} // namespace wstux

#endif /* _TESTS_RAFT_LEADER_ELECTION_IO_STUB_H_ */
