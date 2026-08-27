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

#ifndef _EXAMPLES_RAFT_COUNTER_CONFIG_H_
#define _EXAMPLES_RAFT_COUNTER_CONFIG_H_

#include <memory>
#include <string>
#include <vector>

#include "raft/io.h"

namespace wstux {
namespace examples {
namespace counter {

class config final
{
public:
    using ptr = std::shared_ptr<config>;

    struct server_config
    {
        using list = std::vector<server_config>;

        raft::server_id_t id = raft::gk_invalid_id;
        bool is_voter = false;
        std::string endpoint;
    };

public:
    const server_config::list& cluster_config() const  { return m_servers; }

    const std::string& endpoint() const { return m_endpoint; }

    raft::server_id_t server_id() const { return m_server_id; }

    raft::logging_handler::severity_level level() const { return m_level; }

    bool load(int argc, char** argv);

private:
    bool parse_args(int argc, char** argv);

    bool parse_config_file();

private:
    std::string m_endpoint;

    raft::server_id_t m_server_id = raft::gk_invalid_id;
    raft::logging_handler::severity_level m_level = raft::logging_handler::severity_level::info;

    server_config::list m_servers;
    std::string m_cfg_file;
};

} // namespace counter
} // namespace examples
} // namespace wstux

#endif /* _EXAMPLES_RAFT_COUNTER_CONFIG_H_ */
