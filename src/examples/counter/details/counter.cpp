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

#include "counter/counter.h"

namespace wstux {
namespace examples {
namespace counter {

counter::counter(const config::ptr& p_config)
    : cluster::node(p_config->level())
    , m_p_config(p_config)
    , m_counter(0)
    , m_logger(m_p_config->level())
{}

counter::~counter()
{}

raft::cluster_config counter::bootstrap() const
{
    return m_p_config->cluster_config();
}

raft::config counter::config() const
{
    return m_p_config->configuration();
}

bool counter::execute_follower()
{
    raft::buffer_type buf = get_value();
    if (! buf.empty()) {
        if (buf.size() != sizeof(uint64_t)) {
            return false;
        }
        const uint64_t* p_counter = reinterpret_cast<const uint64_t*>(buf.data());
        m_counter = *p_counter;
        if (m_counter % 10 == 0) {
            LOG_INFO(m_logger, "Got counter value " << m_counter);
        }
    }
    return true;
}

bool counter::execute_leader()
{
    ++m_counter;
    const uint64_t counter = m_counter;
    const char* ptr = reinterpret_cast<const char*>(&counter);
    raft::buffer_type buf(ptr, ptr + sizeof(uint64_t));
    apply(std::move(buf));
    if (m_counter % 10 == 0) {
        LOG_INFO(m_logger, "Counter value " << m_counter);
    }
    return true;
}

std::string counter::join_address() const
{
    return m_p_config->join_address();
}

bool counter::init()
{
    raft::buffer_type buf = get_value();
    if (! buf.empty()) {
        if (buf.size() != sizeof(uint64_t)) {
            return false;
        }
        const uint64_t* p_counter = reinterpret_cast<const uint64_t*>(buf.data());
        m_counter = *p_counter;
        LOG_INFO(m_logger, "Counter value " << m_counter);
    }
    return true;
}

raft::logging_handler::severity_level counter::log_level() const
{
    return m_p_config->level();
}

raft::server_id_t counter::server_id() const
{
    return m_p_config->server_id();
}

std::string counter::work_dir() const
{
    return m_p_config->work_dir();
}

} // namespace counter
} // namespace examples
} // namespace wstux
