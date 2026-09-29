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
#include <chrono>

#include "raft/details/handlers/append_entries_handler.h"
#include "raft/details/replication/entries.h"
#include "raft/details/replication/promotion.h"

namespace wstux {
namespace raft {
namespace details {
namespace replication {
namespace promotion {
namespace {

size_t time_ms()
{
    using clock_t = std::chrono::steady_clock;
    using time_point_t = std::chrono::time_point<clock_t>;

    time_point_t cur = clock_t::now();
    std::chrono::duration<size_t, std::milli> cur_ms = std::chrono::duration_cast<std::chrono::milliseconds>(cur.time_since_epoch());
    return static_cast<std::size_t>(cur_ms.count());
}

} // <anonymous> namespace

void begin(context& ctx)
{
    if (ctx.state.configuration_uncommitted_index != 0) {
        return;
    }
    if (ctx.role.leader.promotee_id == gk_invalid_id) {
        return;
    }

    ctx.role.leader.round = 1;
    ctx.role.leader.round_index = ctx.log.last_index();
    ctx.role.leader.round_start_ms = time_ms();
}

bool catchup_round(context& ctx)
{
    assert(ctx.role.is_leader());
    assert(ctx.role.leader.promotee_id != gk_invalid_id);

    const peer::ptr p_peer = utils::find_peer(ctx, ctx.role.leader.promotee_id);
    assert(p_peer != nullptr);

    const index_t match_index = p_peer->match_index;

    // If the server did not reach the target index for this round, it did not catch up.
    if (match_index < ctx.role.leader.round_index) {
        RAFT_LOG_TRACE(ctx, "Member %llu not yet caught up log. Member index %u, round index %u",
            ctx.role.leader.promotee_id, match_index, ctx.role.leader.round_index);
        return false;
    }

    const index_t last_index = ctx.log.last_index();
    const size_t cur_time_ms = time_ms();
    const size_t round_duration = cur_time_ms - ctx.role.leader.round_start_ms;

    const bool is_up_to_date = match_index == last_index;
    const bool is_fast_enough = round_duration < ctx.election_interval_ms;

    // If the members's log is up-to-date or the round was fast enough, then the server as caught up.
    if (is_up_to_date || is_fast_enough) {
        ctx.role.leader.round = 0;
        ctx.role.leader.round_index = 0;
        ctx.role.leader.round_start_ms = 0;
        return true;
    }

    // Current catch-up round is complete, but there are more entries to replicate, or it was not fast enough
    ++ctx.role.leader.round;
    ctx.role.leader.round_index = last_index;
    ctx.role.leader.round_start_ms = cur_time_ms;
    return false;
}

void process(context& ctx, const server_id_t server_id)
{
    assert(ctx.role.is_leader());
    assert(server_id != gk_invalid_id);

    if (ctx.role.leader.promotee_id != server_id) {
        return;
    }

    const bool is_up_to_date = catchup_round(ctx);
    if (! is_up_to_date) {
        return;
    }

    assert(ctx.role.leader.promotee_id != gk_invalid_id);

    server_config* p_cfg = utils::find_server_config(ctx, ctx.role.leader.promotee_id);
    assert(p_cfg != nullptr);
    assert(! p_cfg->is_voter);

    // Update current configuration.
    p_cfg->is_voter = true;

    const index_t index = ctx.log.last_index() + 1;
    ctx.log.append_change(ctx.term, ctx.state.cluster_cfg);

    const bool accept = entries::replicate(ctx, index);
    if (! accept) {
        return;
    }

    append_entries::request(ctx);

    ctx.role.leader.promotee_id = gk_invalid_id;
    ctx.state.configuration_uncommitted_index = ctx.log.last_index();
}

} // namespace promotion
} // namespace replication
} // namespace details
} // namespace raft
} // namespace wstux
