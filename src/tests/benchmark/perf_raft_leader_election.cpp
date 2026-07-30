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

#include <vector>

#include <benchmark/benchmark.h>

#include "raft_le/server.h"

#include "stub/network_stub.h"

template<size_t N>
static void leader_election(benchmark::State& state)
{
    namespace raft = ::wstux::raft::le;
    namespace tests = ::wstux::raft::le::tests;

    for (auto _ : state) {
        state.PauseTiming();

        tests::network_stub::ptr p_network = std::make_shared<tests::network_stub>(tests::client_type::threaded);
        std::vector<std::pair<raft::server_id_t, bool>> cluster;
        cluster.reserve(N);
        for (size_t i = 0; i < N; ++i) {
            cluster.emplace_back(i + 1, true);
        }
        p_network->create_cluster(cluster, false);
        for (size_t i = 0; i < N; ++i) {
            tests::io_stub::ptr p_io = p_network->get_io(i + 1);
            p_io->m_cfg.heartbeat_interval_ms = 5;
            p_io->m_cfg.vote_timeout_min_ms = 10;
            p_io->m_cfg.vote_timeout_max_ms = 15;
        }
        p_network->init();
        //p_network->create_cluster(cluster);

        state.ResumeTiming();
        p_network->start();
        p_network->wait_leader();
        state.PauseTiming();

        p_network->stop();
        p_network.reset();
        benchmark::DoNotOptimize(p_network);
    }
}

BENCHMARK(leader_election<3>)->Unit(benchmark::kMillisecond);
BENCHMARK(leader_election<5>)->Unit(benchmark::kMillisecond);
BENCHMARK(leader_election<7>)->Unit(benchmark::kMillisecond);

BENCHMARK_MAIN();
