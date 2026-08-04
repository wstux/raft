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

#include "raft_le/details/connection/messages.h"
#include "raft_le/details/connection/serialization.h"

static void serialize_message(benchmark::State& state)
{
    namespace raft = ::wstux::raft::le;

    raft::details::message msg;
    msg.type = raft::details::message_type::vote_response;
    msg.src_id = 1;
    msg.dst_id = 2;
    msg.term = 14;
    msg.vote_resp.accept = false;
    msg.vote_resp.is_prevote = false;

    for (auto _ : state) {
        raft::details::buffer_data_type buf;
        raft::buffer_type buffer = raft::details::serialize(msg, buf);

        benchmark::DoNotOptimize(buf);
        benchmark::DoNotOptimize(buffer);
        benchmark::ClobberMemory();
    }
}
BENCHMARK(serialize_message);

BENCHMARK_MAIN();
