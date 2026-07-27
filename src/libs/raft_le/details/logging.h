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

#ifndef _LIBS_RAFT_LEADER_ELECTION_LOGGING_H_
#define _LIBS_RAFT_LEADER_ELECTION_LOGGING_H_

#include <sstream>

#define _RAFT_LOG(logger, level, VARS)                                      \
    do {                                                                    \
        if (! logger.can_## level ##_log_fn()) {                            \
            break;                                                          \
        }                                                                   \
        std::stringstream ss;                                               \
        ss << VARS << std::endl;                                            \
        logger.log_## level ##_fn(ss.str());                                \
    }                                                                       \
    while (0)

#define RAFT_LOG_ERROR(logger, VARS)    _RAFT_LOG(logger, error,   VARS)
#define RAFT_LOG_WARN(logger,  VARS)    _RAFT_LOG(logger, warning, VARS)
#define RAFT_LOG_INFO(logger,  VARS)    _RAFT_LOG(logger, info,    VARS)
#define RAFT_LOG_DEBUG(logger, VARS)    _RAFT_LOG(logger, debug,   VARS)
#define RAFT_LOG_TRACE(logger, VARS)    _RAFT_LOG(logger, trace,   VARS)

#define RAFT_ROOT_LOG_ERROR(ctx, VARS)  RAFT_LOG_ERROR(ctx.l.root, VARS)
#define RAFT_ROOT_LOG_WARN(ctx,  VARS)  RAFT_LOG_WARN(ctx.l.root,  VARS)
#define RAFT_ROOT_LOG_INFO(ctx,  VARS)  RAFT_LOG_INFO(ctx.l.root,  VARS)
#define RAFT_ROOT_LOG_DEBUG(ctx, VARS)  RAFT_LOG_DEBUG(ctx.l.root, VARS)
#define RAFT_ROOT_LOG_TRACE(ctx, VARS)  RAFT_LOG_TRACE(ctx.l.root, VARS)

#define RAFT_HB_LOG_ERROR(ctx, VARS)    RAFT_LOG_ERROR(ctx.l.hb, VARS)
#define RAFT_HB_LOG_WARN(ctx,  VARS)    RAFT_LOG_WARN(ctx.l.hb,  VARS)
#define RAFT_HB_LOG_INFO(ctx,  VARS)    RAFT_LOG_INFO(ctx.l.hb,  VARS)
#define RAFT_HB_LOG_DEBUG(ctx, VARS)    RAFT_LOG_DEBUG(ctx.l.hb, VARS)
#define RAFT_HB_LOG_TRACE(ctx, VARS)    RAFT_LOG_TRACE(ctx.l.hb, VARS)

#define RAFT_TO_LOG_ERROR(ctx, VARS)    RAFT_LOG_ERROR(ctx.l.to, VARS)
#define RAFT_TO_LOG_WARN(ctx,  VARS)    RAFT_LOG_WARN(ctx.l.to,  VARS)
#define RAFT_TO_LOG_INFO(ctx,  VARS)    RAFT_LOG_INFO(ctx.l.to,  VARS)
#define RAFT_TO_LOG_DEBUG(ctx, VARS)    RAFT_LOG_DEBUG(ctx.l.to, VARS)
#define RAFT_TO_LOG_TRACE(ctx, VARS)    RAFT_LOG_TRACE(ctx.l.to, VARS)

#define RAFT_VOTE_LOG_ERROR(ctx, VARS)  RAFT_LOG_ERROR(ctx.l.vote, VARS)
#define RAFT_VOTE_LOG_WARN(ctx,  VARS)  RAFT_LOG_WARN(ctx.l.vote,  VARS)
#define RAFT_VOTE_LOG_INFO(ctx,  VARS)  RAFT_LOG_INFO(ctx.l.vote,  VARS)
#define RAFT_VOTE_LOG_DEBUG(ctx, VARS)  RAFT_LOG_DEBUG(ctx.l.vote, VARS)
#define RAFT_VOTE_LOG_TRACE(ctx, VARS)  RAFT_LOG_TRACE(ctx.l.vote, VARS)

#endif /* _LIBS_RAFT_LEADER_ELECTION_LOGGING_H_ */
