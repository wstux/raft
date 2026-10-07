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

#ifndef _EXAMPLES_COUNTER_COUNTER_H_
#define _EXAMPLES_COUNTER_COUNTER_H_

#include "counter/config.h"
#include "counter/details/logging.h"
#include "counter/details/node.h"

namespace wstux {
namespace examples {
namespace counter {

class counter final : public cluster::node
{
public:
    explicit counter(const config::ptr& p_config);

    ~counter();

protected:
    virtual raft::cluster_config bootstrap() const;

    virtual raft::config config() const;

    virtual bool execute_follower();

    virtual bool execute_leader();

    virtual std::string join_address() const;

    virtual bool init();

    virtual raft::logging_handler::severity_level log_level() const;

    virtual raft::server_id_t server_id() const;

    virtual std::string work_dir() const;

private:
    config::ptr m_p_config;

    std::atomic_uint64_t m_counter;

    cluster::logging_handler m_logger;
};

} // namespace counter
} // namespace examples
} // namespace wstux

#endif /* _EXAMPLES_RAFT_COUNTER_COUNTER_H_ */
