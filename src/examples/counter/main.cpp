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

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

#include "raft/io.h"

#include "counter/config.h"
#include "counter/counter.h"

void print_usage()
{
    std::cout << "Usage: <program> -i/--id <server_id(>0)> -l/--level <trace/debug/info/warning/error> -c/--config <cfg_file>" << std::endl;
}

int main(int argc, char** argv)
{
    namespace raft = ::wstux::raft;

    ::wstux::examples::counter::config::ptr p_config = std::make_shared<::wstux::examples::counter::config>();

    if (! p_config->load(argc, argv)) {
        print_usage();
        return 1;
    }

    wstux::examples::counter::counter_node node(p_config);
    const int rc = node.run();
    return rc;
}
