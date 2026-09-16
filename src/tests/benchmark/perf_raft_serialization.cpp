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

#include "raft/details/connection/messages.h"
#include "raft/details/connection/serialization.h"

static void serialize_cluster_config(benchmark::State& state)
{
    namespace raft = ::wstux::raft;

    raft::cluster_config conf;
    for (size_t i = 0; i < 7; ++i) {
        conf.servers.emplace_back(i + 1, std::to_string(i + 1), (i == 0));
    }

    for (auto _ : state) {
        raft::buffer_type buffer = raft::details::serialize(conf);

        benchmark::DoNotOptimize(buffer);
        benchmark::ClobberMemory();
    }
}

static void serialize_append_entries_request_message(benchmark::State& state)
{
    namespace raft = ::wstux::raft;

    raft::details::message msg(raft::details::message_type::append_entries_request);
    msg.src_id = 1;
    msg.dst_id = 2;
    msg.term = 14;
    msg.append_entries_req.prev_log_index = 2;
    msg.append_entries_req.prev_log_term = 14;
    msg.append_entries_req.leader_commit = 3;

    for (size_t i = 0; i < 16; ++i) {
        msg.append_entries_req.entries.emplace_back(std::make_shared<raft::entry>());
        msg.append_entries_req.entries.back()->term = 14;
        msg.append_entries_req.entries.back()->type = raft::entry_type::command;
        const char* ptr = reinterpret_cast<const char*>(&i);
        msg.append_entries_req.entries.back()->buffer.assign(ptr, ptr + sizeof(size_t));
    }

    for (auto _ : state) {
        raft::buffer_type buffer = raft::details::serialize(msg);

        benchmark::DoNotOptimize(buffer);
        benchmark::ClobberMemory();
    }
}

static void serialize_append_entries_response_message(benchmark::State& state)
{
    namespace raft = ::wstux::raft;

    raft::details::message msg(raft::details::message_type::append_entries_response);
    msg.src_id = 1;
    msg.dst_id = 2;
    msg.term = 14;
    msg.append_entries_resp.accept = true;
    msg.append_entries_resp.last_log_index = 14;

    for (auto _ : state) {
        raft::buffer_type buffer = raft::details::serialize(msg);

        benchmark::DoNotOptimize(buffer);
        benchmark::ClobberMemory();
    }
}

static void serialize_snapshot_request_message(benchmark::State& state)
{
    namespace raft = ::wstux::raft;

    raft::details::message msg(raft::details::message_type::snapshot_request);
    msg.src_id = 1;
    msg.dst_id = 2;
    msg.term = 14;
    msg.snapshot_req.last_index = 5;
    msg.snapshot_req.last_term = 14;
    msg.snapshot_req.conf.servers = {};  //!< Config as of last_index.
    for (size_t i = 0; i < 7; ++i) {
        msg.snapshot_req.conf.servers.emplace_back(i + 1, std::to_string(i + 1), (i == 0));
    }
    msg.snapshot_req.conf_index = 5;

    const size_t value = 7;
    const char* ptr = reinterpret_cast<const char*>(&value);
    msg.snapshot_req.buffer.assign(ptr, ptr + sizeof(size_t));

    for (auto _ : state) {
        raft::buffer_type buffer = raft::details::serialize(msg);

        benchmark::DoNotOptimize(buffer);
        benchmark::ClobberMemory();
    }
}

static void serialize_vote_request_message(benchmark::State& state)
{
    namespace raft = ::wstux::raft;

    raft::details::message msg(raft::details::message_type::vote_request);
    msg.src_id = 1;
    msg.dst_id = 2;
    msg.term = 14;
    msg.vote_req.is_prevote = false;
    msg.vote_req.last_log_index = 3;
    msg.vote_req.last_log_term = 14;

    for (auto _ : state) {
        raft::buffer_type buffer = raft::details::serialize(msg);

        benchmark::DoNotOptimize(buffer);
        benchmark::ClobberMemory();
    }
}

static void serialize_vote_response_message(benchmark::State& state)
{
    namespace raft = ::wstux::raft;

    raft::details::message msg(raft::details::message_type::vote_response);
    msg.src_id = 1;
    msg.dst_id = 2;
    msg.term = 14;
    msg.vote_resp.accept = false;
    msg.vote_resp.is_prevote = false;

    for (auto _ : state) {
        raft::buffer_type buffer = raft::details::serialize(msg);

        benchmark::DoNotOptimize(buffer);
        benchmark::ClobberMemory();
    }
}

BENCHMARK(serialize_cluster_config);

BENCHMARK(serialize_append_entries_request_message);
BENCHMARK(serialize_append_entries_response_message);

BENCHMARK(serialize_snapshot_request_message);

BENCHMARK(serialize_vote_request_message);
BENCHMARK(serialize_vote_response_message);

BENCHMARK_MAIN();
