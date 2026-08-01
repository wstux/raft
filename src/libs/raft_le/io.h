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

#ifndef _LIBS_RAFT_LEADER_ELECTION_IO_H_
#define _LIBS_RAFT_LEADER_ELECTION_IO_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace wstux {
namespace raft {
namespace le {

using is_stop_fn_t = std::function<bool(void)>;

using server_id_t = uint64_t;
using term_t = uint32_t;

using buffer_type = std::vector<char>;

constexpr server_id_t gk_invalid_id = 0;

struct config final
{
    using ptr = std::shared_ptr<config>;

    size_t vote_timeout_min_ms = 250;
    size_t vote_timeout_max_ms = 500;

    size_t heartbeat_interval_ms = 100;
    size_t heartbeat_probes_count = 10;

    size_t scheduler_threads_count = 4;
};

struct server_config final
{
    server_config()
        : id(gk_invalid_id)
        , is_voter(false)
    {}

    server_config(const server_id_t id, const std::string& endpoint, bool is_voter)
        : id(id)
        , endpoint(endpoint)
        , is_voter(is_voter)
    {}

    server_id_t id;
    std::string endpoint;
    bool is_voter;
};

struct cluster_config final
{
    std::vector<server_config> servers;
};

class iclient
{
public:
    using ptr = std::shared_ptr<iclient>;

public:
    virtual ~iclient() {}

    virtual void send(const buffer_type& msg) = 0;
};

class io
{
public:
    using ptr = std::shared_ptr<io>;

public:
    virtual ~io() {}

    virtual cluster_config bootstrap() const = 0;

    virtual config configuration() const = 0;

    virtual iclient::ptr create_client(server_id_t id, const std::string& endpoint) const = 0;

    virtual void deinit() = 0;

    virtual bool init(server_id_t id) = 0;

    virtual bool load() = 0;

    virtual term_t load_term() = 0;

    virtual void set_term(term_t term) = 0;

    virtual void set_voted_for(server_id_t id) = 0;

    virtual server_id_t voted_for() const = 0;
};

struct logger
{
    using can_log_fn_t = std::function<bool()>;
    using log_fn_t     = std::function<int(const std::string&)>;

    logger()
        : can_error_log_fn([]() -> bool { return false; })
        , log_error_fn([](const std::string&) -> int { return 0; })
        , can_warning_log_fn([]() -> bool { return false; })
        , log_warning_fn([](const std::string&) -> int { return 0; })
        , can_info_log_fn([]() -> bool { return false; })
        , log_info_fn([](const std::string&) -> int { return 0; })
        , can_debug_log_fn([]() -> bool { return false; })
        , log_debug_fn([](const std::string&) -> int { return 0; })
        , can_trace_log_fn([]() -> bool { return false; })
        , log_trace_fn([](const std::string&) -> int { return 0; })
    {}

    can_log_fn_t can_error_log_fn;
    log_fn_t log_error_fn;
    can_log_fn_t can_warning_log_fn;
    log_fn_t log_warning_fn;
    can_log_fn_t can_info_log_fn;
    log_fn_t log_info_fn;
    can_log_fn_t can_debug_log_fn;
    log_fn_t log_debug_fn;
    can_log_fn_t can_trace_log_fn;
    log_fn_t log_trace_fn;
};

class ilogger_factory
{
public:
    using ptr = std::shared_ptr<ilogger_factory>;

public:
    virtual ~ilogger_factory() {}

    virtual logger get_logger(const std::string& ch) = 0;
};

} // namespace le
} // namespace raft
} // namespace wstux

#endif /* _LIBS_RAFT_LEADER_ELECTION_IO_H_ */
