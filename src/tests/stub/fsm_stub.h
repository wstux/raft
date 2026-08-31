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

#ifndef _TESTS_RAFT_FSM_STUB_H_
#define _TESTS_RAFT_FSM_STUB_H_

#include "raft/io.h"

namespace wstux {
namespace raft {
namespace tests {

class fsm_stub final : public raft::fsm
{
public:
    using ptr = std::shared_ptr<fsm_stub>;

public:
    explicit fsm_stub(bool result = true)
        : m_result(result)
    {}

    virtual ~fsm_stub() {}
    virtual bool apply(const raft::buffer_type& buf) override { return change(buf); }
    virtual bool snapshot(raft::buffer_type& buf) override { buf = m_buffer; return m_result; }
    virtual bool restore(const raft::buffer_type& buf) override { return change(buf); }

    template<typename T>
    const T* get() const
    {
        if (m_buffer.size() == 0) {
            return nullptr;
        }
        if (sizeof(T) > m_buffer.size()) {
            return nullptr;
        }
        return reinterpret_cast<const T*>(m_buffer.data());
    }

    template<typename T>
    T get_value(const T& dfl) const
    {
        const T* p_value = get<T>();
        if (p_value == nullptr) {
            return dfl;
        }
        return *p_value;
    }

private:
    bool change(const raft::buffer_type& buf)
    {
        if (! m_result) {
            return false;
        }
        m_buffer = buf;
        return true;
    }

private:
    const bool m_result;
    raft::buffer_type m_buffer;
};

} // namespace tests
} // namespace raft
} // namespace wstux

#endif /* _TESTS_RAFT_FSM_STUB_H_ */
