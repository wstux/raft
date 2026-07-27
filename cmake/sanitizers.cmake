# The MIT License
#
# Copyright (c) 2022 wstux
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

function(EnableSanitizers ADDRESS LEAK UNDEFINED_BEHAVIOR THREAD)
    set(_sanitizers "")

    if (ADDRESS)
        message(INFO "Building project with address sanitizer")
        list(APPEND _sanitizers "address")
    endif()

    if (LEAK)
    message(INFO "Building project with leak sanitizer")
        list(APPEND _sanitizers "leak")
    endif()

    if (UNDEFINED_BEHAVIOR)
        message(INFO "Building project with behavior sanitizer")
        list(APPEND _sanitizers "undefined")
    endif()

    if (THREAD)
        if (ADDRESS OR LEAK)
            message(WARNING "Thread sanitizer does not work with address and leak sanitizer")
        else()
            message(INFO "Building project with thread sanitizer")
            list(APPEND _sanitizers "thread")
        endif()
    endif()

    list(JOIN _sanitizers "," _sanitizers)
    if (_sanitizers)
        add_compile_options(-fsanitize=${_sanitizers})
        add_link_options(-fsanitize=${_sanitizers})
    endif()
endfunction()

