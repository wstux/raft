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

#ifndef _LIBS_RAFT_LEADER_ELECTION_SPAN_H_
#define _LIBS_RAFT_LEADER_ELECTION_SPAN_H_

#if __cplusplus < 202002L
    #include <cassert>
    #include <cstdint>
    #include <type_traits>
#else
    #include <span>
#endif

namespace wstux {
namespace raft {
namespace le {
namespace details {

#if __cplusplus < 202002L
/**
 *  \brief  A lightweight implementation of a non-owning view over a contiguous
 *      sequence of elements.
 *
 *  \details    This class is a simplified analog of `std::span` from the C++20
 *      standard. It provides safe and convenient access to fixed-size data
 *      arrays without managing their lifetime.
 *
 *      Reason for creation: the original `std::span` is only available
 *      starting from C++20. This class is designed to ensure compatibility with
 *      legacy codebases and compilers restricted to the C++17 standard, where
 *      the standard implementation is missing.
 *
 *  \tparam T - the type of elements stored in the sequence.
 *  \tparam TExtent - the fixed size of the sequence (number of elements).
 */
template<typename T, std::size_t TExtent>
class span final
{
public:
    using element_type           = T;
    using value_type             = std::remove_cv_t<T>;
    using size_type              = std::size_t;
    using pointer                = T*;
    using reference              = T&;
    using iterator               = T*;

    static constexpr size_type extent = TExtent;

    constexpr span(pointer ptr, [[maybe_unused]] size_type count) : m_data(ptr) { assert(count == TExtent); }
    constexpr span(pointer first, [[maybe_unused]] pointer last) : m_data(first) { assert((last - first) == TExtent); }

    constexpr pointer data() const noexcept { return m_data; }
    constexpr size_type size() const noexcept { return TExtent; }

    constexpr iterator begin() const noexcept { return m_data; }
    constexpr iterator end() const noexcept { return m_data + TExtent; }

private:
    pointer m_data;
};

template<typename T, std::size_t TExtent>
using span_type = span<T, TExtent>;

#else

template<typename T, std::size_t TExtent>
using span_type = std::span<T, TExtent>;

#endif

} // namespace details
} // namespace le
} // namespace raft
} // namespace wstux

#endif /* _LIBS_RAFT_LEADER_ELECTION_SPAN_H_ */
