/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2022 Advanced Micro Devices, Inc. All rights reserved.
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
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#ifndef MIGRAPHX_GUARD_STREAMUTILS_HPP
#define MIGRAPHX_GUARD_STREAMUTILS_HPP

#include <ostream>
#include <algorithm>
#include <migraphx/rank.hpp>
#include <migraphx/config.hpp>

namespace migraphx {
inline namespace MIGRAPHX_INLINE_NS {

template <class T>
struct stream_range_container
{
    const T* r;
    stream_range_container(const T& x) : r(&x) {}

    friend std::ostream& operator<<(std::ostream& os, const stream_range_container& sr)
    {
        assert(sr.r != nullptr);
        if(!sr.r->empty())
        {
            os << sr.r->front();
            std::for_each(
                std::next(sr.r->begin()), sr.r->end(), [&](auto&& x) { os << ", " << x; });
        }
        return os;
    }
};

template <class Range>
inline stream_range_container<Range> stream_range(const Range& r)
{
    return {r};
}

namespace detail {

inline void stream_write_value_impl(rank<2>, std::ostream& os, const std::string& x) { os << x; }

template <class Range>
auto stream_write_value_impl(rank<1>, std::ostream& os, const Range& r)
    -> decltype(r.begin(), r.end(), void())
{
    os << "{";
    os << stream_range(r);
    os << "}";
}

template <class T>
void stream_write_value_impl(rank<0>, std::ostream& os, const T& x)
{
    os << x;
}
} // namespace detail

template <class T>
void stream_write_value(std::ostream& os, const T& x)
{
    detail::stream_write_value_impl(rank<2>{}, os, x);
}

} // namespace MIGRAPHX_INLINE_NS
} // namespace migraphx

#endif
