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

#ifndef _LIBS_RAFT_LEADER_ELECTION_LOGGER_CHANNELS_H_
#define _LIBS_RAFT_LEADER_ELECTION_LOGGER_CHANNELS_H_

#include <string>
#include <vector>

#include "raft_le/io.h"

namespace wstux {
namespace raft {
namespace le {
namespace details {

struct loggers final
{
    explicit loggers(const ilogger_factory::ptr& p_factory)
        : root(p_factory->get_logger(root_channel()))
        , hb(p_factory->get_logger(heartbeat_channel()))
        , to(p_factory->get_logger(timeout_channel()))
        , vote(p_factory->get_logger(vote_channel()))
    {}

    static std::string root_channel() { return "Raft::Root"; }
    static std::string heartbeat_channel() { return "Raft::Heartbeat"; }
    static std::string timeout_channel() { return "Raft::Timeout"; }
    static std::string vote_channel() { return "Raft::Vote"; }

    static std::vector<std::string> logging_channels()
    {
        return {
            root_channel(),
            heartbeat_channel(),
            timeout_channel(),
            vote_channel()
        };
    }

    logger root;    //!< Root logger.
    logger hb;      //!< Heartbeat logger.
    logger to;      //!< Timeout logger.
    logger vote;    //!< Vote logger.
};

} // namespace details
} // namespace le
} // namespace raft
} // namespace wstux

#endif /* _LIBS_RAFT_LEADER_ELECTION_LOGGER_CHANNELS_H_ */
