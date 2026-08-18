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

#include <benchmark/benchmark.h>

#include "raft_le/server.h"
#include "raft_le/details/handlers/heartbeat_handler.h"
#include "raft_le/details/handlers/timeout_handler.h"
#include "raft_le/details/role/convert.h"

#include "stub/empty_io.h"

namespace {

constexpr size_t gk_cluster_size = 7;

::wstux::raft::le::details::context::ptr make_ctx(size_t servs_count)
{
    namespace raft = ::wstux::raft::le;
    namespace details = raft::details;
    namespace tests = raft::tests;

    tests::empty_io::ptr p_io = std::make_shared<tests::empty_io>();
    for (size_t i = 0; i < servs_count; ++i) {
        p_io->cluster_cfg.servers.emplace_back(i + 1, true);
    }

    tests::empty_io* p_raw_io = p_io.get();
    std::function<bool()> is_stop_fn = [p_raw_io]()->bool { return p_raw_io->is_stop; };
    details::context::ptr p_ctx = std::make_unique<details::context>(1, p_io, raft::logging_handler::ptr(), is_stop_fn);
    if (! raft::details::utils::init(*p_ctx, 1)) {
        return nullptr;
    }
    p_ctx->election_task = p_ctx->p_scheduler->make_task(std::bind(&raft::details::timeout::election_timeout_task, std::ref(*p_ctx)));
    p_ctx->heartbeat_task = p_ctx->p_scheduler->make_task(std::bind(&raft::details::timeout::heartbeat_timeout_task, std::ref(*p_ctx)));

    p_ctx->p_scheduler->cancel(p_ctx->election_task);
    p_ctx->p_scheduler->cancel(p_ctx->heartbeat_task);

    raft::details::utils::load(*p_ctx);
    raft::details::role::become_follower(*p_ctx);
    raft::details::role::become_candidate(*p_ctx);
    raft::details::role::become_leader(*p_ctx);

    p_ctx->p_scheduler->cancel(p_ctx->election_task);
    p_ctx->p_scheduler->cancel(p_ctx->heartbeat_task);

    assert(p_ctx->role.is_leader());
    return p_ctx;
}

::wstux::raft::le::details::context::ptr g_p_ctx = make_ctx(gk_cluster_size);

::wstux::raft::le::details::peer::list to_list(::wstux::raft::le::details::context& ctx)
{
    namespace raft = ::wstux::raft::le;

    raft::details::peer::list peers;
    {
        peers.reserve(ctx.peers.size());
        std::transform(ctx.peers.begin(), ctx.peers.end(), std::back_inserter(peers),
            [](const raft::details::peer& p) -> raft::details::peer { return p; });
    }
    return peers;
}

} // <anonymous> namespace

static void check_contact_quorum(benchmark::State& state)
{
    namespace raft = ::wstux::raft::le;

    for (auto _ : state) {
        bool is_check = raft::details::peers::check_contact_quorum(*g_p_ctx);
        benchmark::DoNotOptimize(is_check);
    }
}

static void request_to_list(benchmark::State& state)
{
    namespace raft = ::wstux::raft::le;

    for (auto _ : state) {
        raft::details::peer::list peers = to_list(*g_p_ctx);
        for (const raft::details::peer& p : peers) {
            bool is_voter = p.is_voter;
            benchmark::DoNotOptimize(is_voter);
        }
    }
}

static void request_lock_list(benchmark::State& state)
{
    namespace raft = ::wstux::raft::le;

    for (auto _ : state) {
        for (const raft::details::peer& p : g_p_ctx->peers) {
            bool is_voter = p.is_voter;
            benchmark::DoNotOptimize(is_voter);
        }
    }
}

BENCHMARK(check_contact_quorum)->Threads(1)->Threads(2)->Threads(4)->Threads(8);

BENCHMARK(request_to_list)->Threads(1)->Threads(2)->Threads(4)->Threads(8);
BENCHMARK(request_lock_list)->Threads(1)->Threads(2)->Threads(4)->Threads(8);

BENCHMARK_MAIN();
