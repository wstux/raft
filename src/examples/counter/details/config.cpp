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
namespace {

struct server_config final
{
    std::string endpoint = "";
    raft::server_id_t id = raft::gk_invalid_id;
    bool is_voter = false;
    bool is_async_io = false;
    std::string work_dir = "";
    bool bootstrap = false;
    bool add_to_bootstrap = false;
    std::string join_address = "";
};

} // <anonymous> namespace

bool config::load(int argc, char** argv)
{
    if (! parse_args(argc, argv)) {
        return false;
    }
    if (! parse_config_file()) {
        return false;
    }

    if (m_config.address.empty()) {
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
                m_server_id = static_cast<raft::server_id_t>(std::atol(argv[++i]));
            }
        } else if (arg == "-l" || arg == "--level") {
            if (i + 1 < argc) {
                std::string lvl = argv[++i];
                if (lvl == "trace") {
                    m_level = raft::logging_handler::severity_level::trace;
                } else if (lvl == "debug") {
                    m_level = raft::logging_handler::severity_level::debug;
                } else if (lvl == "info") {
                    m_level = raft::logging_handler::severity_level::info;
                } else if (lvl == "warning") {
                    m_level = raft::logging_handler::severity_level::warning;
                } else if (lvl == "error") {
                    m_level = raft::logging_handler::severity_level::error;
                }
            }
        }
    }
    if (m_cfg_file.empty()) {
        return false;
    }
    if (m_server_id == raft::gk_invalid_id) {
        return false;
    }
    return true;
}

bool config::parse_config_file()
{
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

    std::vector<server_config> configs;

    std::string line;
    while (std::getline(fin, line)) {
        line = trim_fn(line);
        if (line.empty() || line[0] == ';' || line[0] == '#') {
            continue;
        }

        if (line == "[server]") {
            if (! configs.empty()) {
                const server_config& cfg = configs.back();
                if ((cfg.bootstrap && ! cfg.add_to_bootstrap)) {
                    return false;
                } else if (! cfg.bootstrap && cfg.join_address.empty()) {
                    return false;
                } else if (cfg.endpoint.empty() || cfg.work_dir.empty()) {
                    return false;
                } else if (cfg.id == raft::gk_invalid_id) {
                    return false;
                }
            }
            configs.push_back(server_config());
        } else {
            size_t delim_pos = line.find('=');
            if (delim_pos != std::string::npos) {
                std::string key = trim_fn(line.substr(0, delim_pos));
                std::string value = trim_fn(line.substr(delim_pos + 1));
                if (key.empty() || value.empty()) {
                    return false;
                }
                if (key == "endpoint") {
                    configs.back().endpoint = value;
                } else if (key == "id") {
                    configs.back().id = static_cast<raft::server_id_t>(std::stoi(value));
                } else if (key == "is_voter") {
                    configs.back().is_voter = (value == "true");
                } else if (key == "is_async_io") {
                    configs.back().is_async_io = (value == "true");
                } else if (key == "work_dir") {
                    configs.back().work_dir = value;
                } else if (key == "bootstrap") {
                    configs.back().bootstrap = (value == "true");
                } else if (key == "add_to_bootstrap") {
                    configs.back().add_to_bootstrap = (value == "true");
                } else if (key == "join_address") {
                    configs.back().join_address = value;
                }
            }
        }
    }

    std::vector<server_config>::const_iterator it = std::find_if(configs.cbegin(), configs.cend(),
        [this](const server_config& cfg) -> bool { return m_server_id == cfg.id; });
    if (it == configs.cend()) {
        return false;
    }

    const server_config& cfg = *it;
    m_config.address = cfg.endpoint;
    m_config.is_voter = cfg.is_voter;
    m_config.is_async_io = cfg.is_async_io;

    if (cfg.bootstrap) {
        for (const server_config& cfg : configs) {
            if (cfg.add_to_bootstrap) {
                m_cluster_cfg.servers.emplace_back(cfg.id, cfg.endpoint, cfg.is_voter);
            }
        }
    } else {
        m_join_address = cfg.join_address;
        if (m_join_address.empty()) {
            return false;
        }
    }

    m_work_dir = cfg.work_dir;
    if (m_work_dir.empty()) {
        return false;
    }

    return true;
}

} // namespace counter
} // namespace examples
} // namespace wstux
