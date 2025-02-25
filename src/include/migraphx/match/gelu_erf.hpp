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
#ifndef MIGRAPHX_GUARD_MATCH_GELU_ERF_HPP
#define MIGRAPHX_GUARD_MATCH_GELU_ERF_HPP

#include <migraphx/config.hpp>
#include <migraphx/matcher.hpp>

namespace migraphx {
inline namespace MIGRAPHX_INLINE_NS {
namespace match {

namespace detail {
template <class F>
struct gelu_erf_matcher
{
    F f;
    auto erf_fn() const
    {
        return f("erf")(
            used_once(),
            arg(0)(used_once(),
                   f("mul")(either_arg(0, 1)(none_of(has_value(M_SQRT1_2, 1e-3)).bind("x"),
                                             has_value(M_SQRT1_2, 1e-3)))));
    }

    auto add_erf() const
    {
        return f("add")(used_once(), either_arg(0, 1)(erf_fn(), has_value(1.0f)));
    }

    auto one_half() const { return has_value(0.5f); }

    auto matcher() const { return unordered_tree(f("mul"), one_half(), add_erf(), any()); }
};
} // namespace detail

template <class F>
auto gelu_erf(F f)
{
    return detail::gelu_erf_matcher<F>{f}.matcher();
}

inline auto gelu_erf()
{
    return gelu_erf([](auto x) { return name(x); });
}

} // namespace match
} // namespace MIGRAPHX_INLINE_NS
} // namespace migraphx
#endif // MIGRAPHX_GUARD_MATCH_GELU_ERF_HPP
