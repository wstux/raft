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

#ifndef _EXAMPLES_RAFT_COUNTER_FSM_H_
#define _EXAMPLES_RAFT_COUNTER_FSM_H_

#include <atomic>

#include "raft/io.h"

namespace wstux {
namespace examples {
namespace counter {
namespace details {

class fsm final : public raft::fsm
{
public:
    using ptr = std::shared_ptr<fsm>;

public:
    virtual ~fsm() {}

    virtual bool apply(const raft::buffer_type& buf) noexcept { return change(buf); }

    virtual bool restore(const raft::buffer_type& buf) noexcept { return change(buf); }

    virtual bool take_snapshot(raft::buffer_type& buf) noexcept { buf = m_buffer; return true; }

    uint64_t get_counter() const { return m_counter; }

private:
    bool change(const raft::buffer_type& buf)
    {
        const uint64_t* p_counter = get_ptr(buf);
        if (p_counter != nullptr) {
            m_buffer = buf;
            m_counter = *p_counter;
            return true;
        }
        return false;
    }

    const uint64_t* get_ptr(const raft::buffer_type& buf) const
    {
        if (buf.size() == 0) {
            return nullptr;
        }
        if (sizeof(uint64_t) > buf.size()) {
            return nullptr;
        }
        return reinterpret_cast<const uint64_t*>(buf.data());
    }

private:
    std::atomic_uint64_t m_counter = 0;
    raft::buffer_type m_buffer;
};

} // namespace details
} // namespace counter
} // namespace examples
} // namespace wstux

#endif /* _EXAMPLES_RAFT_COUNTER_FSM_H_ */
