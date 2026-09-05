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

#include "raft/server.h"

#include "stub/network_stub.h"

template<size_t N>
static void add_member(benchmark::State& state)
{
    namespace raft = ::wstux::raft;
    namespace tests = ::wstux::raft::tests;

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
        p_network->start();
        p_network->wait_leader();

        raft::server::ptr p_leader = p_network->get_leader();
        p_network->create_server(N + 1, true);

        state.ResumeTiming();
        p_leader->add(N + 1, std::to_string(N + 1), true);
        p_network->wait_changed_cluster_cfg();
        state.PauseTiming();

        p_network->stop();
        p_network.reset();
        benchmark::DoNotOptimize(p_network);
    }
}

template<size_t N>
static void remove_member(benchmark::State& state)
{
    namespace raft = ::wstux::raft;
    namespace tests = ::wstux::raft::tests;

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
        p_network->start();
        p_network->wait_leader();

        raft::server::ptr p_leader = p_network->get_leader();
        raft::server_id_t remove_id = (p_leader->id() == N) ? 1 : (p_leader->id() + 1);

        state.ResumeTiming();
        p_leader->remove(remove_id);
        p_network->wait_changed_cluster_cfg_except(remove_id);
        state.PauseTiming();

        p_network->stop();
        p_network.reset();
        benchmark::DoNotOptimize(p_network);
    }
}

template<size_t N>
static void add_member_default(benchmark::State& state)
{
    namespace raft = ::wstux::raft;
    namespace tests = ::wstux::raft::tests;

    for (auto _ : state) {
        state.PauseTiming();

        tests::network_stub::ptr p_network = std::make_shared<tests::network_stub>(tests::client_type::threaded);
        std::vector<std::pair<raft::server_id_t, bool>> cluster;
        cluster.reserve(N);
        for (size_t i = 0; i < N; ++i) {
            cluster.emplace_back(i + 1, true);
        }
        p_network->create_cluster(cluster);
        p_network->start();
        p_network->wait_leader();

        raft::server::ptr p_leader = p_network->get_leader();
        p_network->create_server(N + 1, true);

        state.ResumeTiming();
        p_leader->add(N + 1, std::to_string(N + 1), true);
        p_network->wait_changed_cluster_cfg();
        state.PauseTiming();

        p_network->stop();
        p_network.reset();
        benchmark::DoNotOptimize(p_network);
    }
}

template<size_t N>
static void remove_member_default(benchmark::State& state)
{
    namespace raft = ::wstux::raft;
    namespace tests = ::wstux::raft::tests;

    for (auto _ : state) {
        state.PauseTiming();

        tests::network_stub::ptr p_network = std::make_shared<tests::network_stub>(tests::client_type::threaded);
        std::vector<std::pair<raft::server_id_t, bool>> cluster;
        cluster.reserve(N);
        for (size_t i = 0; i < N; ++i) {
            cluster.emplace_back(i + 1, true);
        }
        p_network->create_cluster(cluster);
        p_network->start();
        p_network->wait_leader();

        raft::server::ptr p_leader = p_network->get_leader();
        const raft::server_id_t remove_id = (p_leader->id() == N) ? 1 : (p_leader->id() + 1);

        state.ResumeTiming();
        p_leader->remove(remove_id);
        p_network->wait_changed_cluster_cfg_except(remove_id);
        state.PauseTiming();

        p_network->stop();
        p_network.reset();
        benchmark::DoNotOptimize(p_network);
    }
}

BENCHMARK(add_member<3>)->Unit(benchmark::kMillisecond);
BENCHMARK(add_member<5>)->Unit(benchmark::kMillisecond);
BENCHMARK(add_member<7>)->Unit(benchmark::kMillisecond);

BENCHMARK(remove_member<3>)->Unit(benchmark::kMillisecond);
BENCHMARK(remove_member<5>)->Unit(benchmark::kMillisecond);
BENCHMARK(remove_member<7>)->Unit(benchmark::kMillisecond);

BENCHMARK(add_member_default<3>)->Unit(benchmark::kMillisecond);
BENCHMARK(add_member_default<5>)->Unit(benchmark::kMillisecond);
BENCHMARK(add_member_default<7>)->Unit(benchmark::kMillisecond);

BENCHMARK(remove_member_default<3>)->Unit(benchmark::kMillisecond);
BENCHMARK(remove_member_default<5>)->Unit(benchmark::kMillisecond);
BENCHMARK(remove_member_default<7>)->Unit(benchmark::kMillisecond);

BENCHMARK_MAIN();
