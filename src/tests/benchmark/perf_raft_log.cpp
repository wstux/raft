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

#include <benchmark/benchmark.h>

#include "raft/details/log/log.h"

namespace {

namespace raft = ::wstux::raft;
namespace details = raft::details;

raft::buffer_type make_test_buffer(const size_t value = 64)
{
    const char* ptr = reinterpret_cast<const char*>(&value);
    return raft::buffer_type(ptr, ptr + sizeof(size_t));
}

void fill_log(details::log_store& l, size_t count)
{
    raft::cluster_config cfg;
    cfg.servers.emplace_back(raft::server_config(1, "127.0.0.1:8001", true));

    for (size_t i = 0; i < count; ++i) {
        if (i % 2 == 0) {
            l.append_command(1, make_test_buffer());
        } else {
            l.append_change(1, cfg);
        }
    }
}


} // <anonymous> namespace

static void log_append_command(benchmark::State& state)
{
    for (auto _ : state) {
        state.PauseTiming();
        details::log_store log;
        log.offset = 1;
        raft::buffer_type buf = make_test_buffer();
        state.ResumeTiming();

        for (int i = 0; i < state.range(0); ++i) {
            log.append_command(1, buf);
        }
    }
    state.SetItemsProcessed(state.iterations() * state.range(0));
}

static void log_append_change(benchmark::State& state)
{
    for (auto _ : state) {
        state.PauseTiming();
        details::log_store log;
        log.offset = 1;

        raft::cluster_config cfg;
        cfg.servers.emplace_back(raft::server_config(1, "127.0.0.1:8001", true));
        state.ResumeTiming();

        for (int i = 0; i < state.range(0); ++i) {
            log.append_change(1, cfg);
        }
    }
    state.SetItemsProcessed(state.iterations() * state.range(0));
}

static void log_append(benchmark::State& state)
{
    for (auto _ : state) {
        state.PauseTiming();
        details::log_store log;
        log.offset = 1;

        raft::entry::ptr p_entry = std::make_shared<raft::entry>();
        p_entry->term = 1;
        p_entry->type = raft::entry_type::command;
        p_entry->buffer = make_test_buffer();
        state.ResumeTiming();

        log.append(p_entry);
    }
}

static void log_get_entry(benchmark::State& state)
{
    details::log_store log;
    log.offset = 1;
    fill_log(log, state.range(0));
    raft::index_t target_idx = state.range(0) / 2;

    for (auto _ : state) {
        raft::entry::ptr p_entry = log.get_entry(target_idx);
        benchmark::DoNotOptimize(p_entry);
    }
}

static void log_acquire(benchmark::State& state)
{
    details::log_store log;
    log.offset = 1;
    fill_log(log, 10000);

    for (auto _ : state) {
        raft::entry::list entries = log.acquire(100);
        benchmark::DoNotOptimize(entries);
    }
}

static void log_get(benchmark::State& state)
{
    details::log_store log;
    log.load(0, 0, 1);
    fill_log(log, 1000);

    for (auto _ : state) {
        raft::index_t last_index = log.last_index();
        raft::term_t last_term = log.last_term();
        raft::term_t term = log.term(500);
        benchmark::DoNotOptimize(last_index);
        benchmark::DoNotOptimize(last_term);
        benchmark::DoNotOptimize(term);
    }
}

static void log_truncate(benchmark::State& state)
{
    for (auto _ : state) {
        state.PauseTiming();
        details::log_store log;
        log.offset = 1;
        fill_log(log, state.range(0));
        raft::index_t index = state.range(0) / 2;
        state.ResumeTiming();

        log.truncate(index);
    }
}

static void log_take_snapshot(benchmark::State& state)
{
    for (auto _ : state) {
        state.PauseTiming();
        details::log_store log;
        log.offset = 1;
        log.snapshot.last_index = 1;
        log.snapshot.last_term = 1;
        fill_log(log, 2000);
        state.ResumeTiming();

        log.take_snapshot(1500, 100);
    }
}

static void log_load_restore(benchmark::State& state)
{
    for (auto _ : state) {
        details::log_store log;
        log.load(500, 2, 501);
        log.restore(1000, 3);
    }
}

BENCHMARK(log_append_command)->Arg(10)->Arg(100)->Arg(1000);
BENCHMARK(log_append_change)->Arg(10)->Arg(100);
BENCHMARK(log_append);

BENCHMARK(log_get_entry)->Arg(100)->Arg(10000);

BENCHMARK(log_acquire);

BENCHMARK(log_get);

BENCHMARK(log_truncate)->Arg(100)->Arg(1000);

BENCHMARK(log_take_snapshot);

BENCHMARK(log_load_restore);

BENCHMARK_MAIN();
