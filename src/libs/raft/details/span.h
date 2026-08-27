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

#ifndef _LIBS_RAFT_SPAN_H_
#define _LIBS_RAFT_SPAN_H_

#if __cplusplus < 202002L
    #include <cassert>
    #include <cstdint>
    #include <type_traits>
#else
    #include <span>
#endif

namespace wstux {
namespace raft {
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
 */
template<typename T>
class span final
{
public:
    using element_type           = T;
    using value_type             = std::remove_cv_t<T>;
    using size_type              = std::size_t;
    using pointer                = T*;
    using reference              = T&;
    using iterator               = T*;

    constexpr span(pointer ptr, size_type count)
        : m_data(ptr)
        , m_size(count)
    {}

    constexpr span(pointer first, pointer last)
        : m_data(first)
        , m_size(last - first)
    {}

    template<std::size_t N>
    constexpr span(T (&arr)[N]) noexcept
        : m_data(arr)
        , m_size(N)
    {}

    template<typename TContainer,
             typename = std::enable_if_t<
                ! std::is_same_v<std::decay_t<TContainer>, span> &&
                std::is_convertible_v<decltype(std::declval<TContainer&>().data()), pointer>
             >>
    constexpr span(TContainer&& cont)
        : m_data(cont.data())
        , m_size(cont.size())
    {}

    constexpr pointer data() const noexcept { return m_data; }
    constexpr size_type size() const noexcept { return m_size; }
    constexpr bool empty() const noexcept { return m_size == 0; }

    constexpr iterator begin() const noexcept { return m_data; }
    constexpr iterator end() const noexcept { return m_data + m_size; }

private:
    pointer m_data = nullptr;
    size_type m_size = 0;
};

template<typename T>
using span_type = span<T>;

#else

template<typename T>
using span_type = std::span<T>;

#endif

} // namespace details
} // namespace raft
} // namespace wstux

#endif /* _LIBS_RAFT_SPAN_H_ */
