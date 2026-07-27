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
#include <algorithm>
#include <chrono>
#include <mutex>

#include "raft_le/details/context.h"

namespace wstux {
namespace raft {
namespace le {
namespace details {
namespace {

bool load_peers(context& /*ctx*/, const cluster_config_t& cluster_cfg)
{
    bool is_loaded = false;
    for (const server_config& _ : cluster_cfg) {
    }
    return is_loaded;
}

} // <anonymous> namespace

////////////////////////////////////////////////////////////////////////////////
// class context

context::context(server_id_t id, const io::ptr p_io, const ilogger_factory::ptr p_factory, const is_stop_fn_t& is_stop)
    : id(id)
    , is_stop_fn(is_stop)
    , is_async_io(false)
    , p_io(p_io)
    , term(0)
    , heartbeat_interval_ms(100)
    , heartbeat_probes_count(10)
    , rand_engine(std::chrono::system_clock::now().time_since_epoch().count() * id)
    , election_distribution(250, 500)
    , l(p_factory)
{}

std::ostream& operator<<(std::ostream& os, const context& /*ctx*/)
{
    return os;
}

namespace sturtup {

bool init(context& ctx, server_id_t id)
{
    assert(ctx.id == id);

    if (! ctx.p_io->init(id)) {
        return false;
    }

    const config& cfg = ctx.p_io->configuration();
    if (cfg.heartbeat_interval_ms == 0 || cfg.vote_timeout_max_ms == 0 || cfg.vote_timeout_max_ms < cfg.vote_timeout_min_ms) {
        return false;
    }

    return true;
}

bool load(context& ctx)
{
    io::ptr p_io = ctx.p_io;
    if (! p_io->load()) {
        return false;
    }

    ctx.term = p_io->load_term();

    const cluster_config_t& cluster_cfg = p_io->bootstrap();
    if (! load_peers(ctx, cluster_cfg)) {
        return false;
    }
    return true;
}

} // namespace sturtup

namespace utils {

size_t current_time_ms()
{
    using clock_t = std::chrono::steady_clock;
    using time_point_t = std::chrono::time_point<clock_t>;

    time_point_t cur = clock_t::now();
    std::chrono::duration<size_t, std::milli> cur_ms = std::chrono::duration_cast<std::chrono::milliseconds>(cur.time_since_epoch());
    return static_cast<std::size_t>(cur_ms.count());
}

cluster_config_t make_config(context& /*ctx*/)
{
    return {};
}

} // namespace utils

} // namespace details
} // namespace le
} // namespace raft
} // namespace wstux
