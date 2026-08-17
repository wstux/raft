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

#include <functional>
#include <fstream>
#include <string_view>

#include "counter/config.h"

namespace wstux {
namespace examples {
namespace counter {

bool config::load(int argc, char** argv)
{
    if (! parse_args(argc, argv)) {
        return false;
    }
    if (! parse_config_file()) {
        return false;
    }

    for (const raft::le::server_config& cfg : m_cluster_config.servers) {
        if (cfg.id == m_server_id) {
            m_endpoint = cfg.endpoint;
            break;
        }
    }
    if (m_endpoint.empty()) {
        return false;
    }
    return true;
}

bool config::parse_args(int argc, char** argv)
{
    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            return false;
        } else if (arg == "-c" || arg == "--config") {
            m_cfg_file = argv[++i];
        } else if (arg == "-i" || arg == "--id") {
            if (i + 1 < argc) {
                m_server_id = static_cast<raft::le::server_id_t>(std::atol(argv[++i]));
            }
        } else if (arg == "-l" || arg == "--level") {
            if (i + 1 < argc) {
                std::string lvl = argv[++i];
                if (lvl == "trace") {
                    m_level = raft::le::logging_handler::severity_level::trace;
                } else if (lvl == "debug") {
                    m_level = raft::le::logging_handler::severity_level::debug;
                } else if (lvl == "info") {
                    m_level = raft::le::logging_handler::severity_level::info;
                } else if (lvl == "warning") {
                    m_level = raft::le::logging_handler::severity_level::warning;
                } else if (lvl == "error") {
                    m_level = raft::le::logging_handler::severity_level::error;
                }
            }
        }
    }
    if (m_cfg_file.empty()) {
        return false;
    }
    if (m_server_id == raft::le::gk_invalid_id) {
        return false;
    }
    return true;
}

bool config::parse_config_file()
{
    const std::function<bool(const raft::le::server_config&)> is_valid_fn =
        [](const raft::le::server_config& cfg) -> bool {
            return (! cfg.endpoint.empty() && cfg.id != raft::le::gk_invalid_id);
        };

    const std::function<std::string(const std::string&)> trim_fn =
        [](const std::string& str) -> std::string {
            size_t first = str.find_first_not_of(" \t\r\n");
            if (first == std::string::npos) {
                return "";
            }
            size_t last = str.find_last_not_of(" \t\r\n");
            return str.substr(first, (last - first + 1));
        };

    std::ifstream fin(m_cfg_file);
    if (! fin.is_open()) {
        return false;
    }

    std::string line;
    while (std::getline(fin, line)) {
        line = trim_fn(line);
        if (line.empty() || line[0] == ';' || line[0] == '#') {
            continue;
        }

        if (line == "[server]") {
            if (! m_cluster_config.servers.empty()) {
                if (! is_valid_fn(m_cluster_config.servers.back())) {
                    return false;
                }
            }
            raft::le::server_config srv_cfg;
            srv_cfg.id = raft::le::gk_invalid_id;
            srv_cfg.is_voter = false;
            m_cluster_config.servers.push_back(srv_cfg);
        } else {
            size_t delim_pos = line.find('=');
            if (delim_pos != std::string::npos) {
                std::string key = trim_fn(line.substr(0, delim_pos));
                std::string value = trim_fn(line.substr(delim_pos + 1));
                if (key.empty() || value.empty()) {
                    return false;
                }
                if (key == "endpoint") {
                    m_cluster_config.servers.back().endpoint = value;
                } else if (key == "id") {
                    m_cluster_config.servers.back().id = static_cast<raft::le::server_id_t>(std::stoi(value));
                } else if (key == "is_voter") {
                    m_cluster_config.servers.back().is_voter = (value == "true");
                }
            }
        }
    }

    return true;
}

} // namespace counter
} // namespace examples
} // namespace wstux
