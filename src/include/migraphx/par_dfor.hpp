#ifndef MIGRAPHX_GUARD_RTGLIB_PAR_DFOR_HPP
#define MIGRAPHX_GUARD_RTGLIB_PAR_DFOR_HPP

#include <migraphx/par_for.hpp>
#include <migraphx/functional.hpp>
#include <array>
#include <numeric>

namespace migraphx {
inline namespace MIGRAPHX_INLINE_NS {

template <class... Ts>
auto par_dfor(Ts... xs)
{
    return [=](auto f) {
        using array_type    = std::array<int, sizeof...(Ts)>;
        array_type lens     = {{static_cast<int>(xs)...}};
        auto n              = std::accumulate(lens.begin(), lens.end(), 1, std::multiplies<int>{});
        const int min_grain = 8;
        if(n > 2 * min_grain)
        {
            array_type strides;
            strides.fill(1);
            std::partial_sum(
                lens.rbegin(), lens.rend() - 1, strides.rbegin() + 1, std::multiplies<int>());
            auto size = std::accumulate(lens.begin(), lens.end(), 1, std::multiplies<int>());
            par_for(size, min_grain, [&](int i) {
                array_type indices;
                std::transform(strides.begin(),
                               strides.end(),
                               lens.begin(),
                               indices.begin(),
                               [&](int stride, int len) { return (i / stride) % len; });
                migraphx::unpack(f, indices);
            });
        }
        else
        {
            dfor(xs...)(f);
        }

    };
}

} // namespace MIGRAPHX_INLINE_NS
} // namespace migraphx

#endif
