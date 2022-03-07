#ifndef MIGRAPHX_GUARD_OPERATORS_SCATTER_ADD_HPP
#define MIGRAPHX_GUARD_OPERATORS_SCATTER_ADD_HPP

#include <array>
#include <migraphx/check_shapes.hpp>
#include <migraphx/stringutils.hpp>
#include <migraphx/streamutils.hpp>
#include <migraphx/shape_for_each.hpp>
#include <migraphx/config.hpp>
#include <migraphx/value.hpp>
#include <migraphx/op/normalize_attribute.hpp>
#include <cmath>
#include <utility>
#include <migraphx/op/scatter.hpp>

// ScatterElement op. with "add" as the reduction attribute.
namespace migraphx {
inline namespace MIGRAPHX_INLINE_NS {
namespace op {

struct scatter_add : scatter<scatter_add>
{

    template <class Self, class F>
    static auto reflect(Self& self, F f)
    {
        return pack(f(self.axis, "axis"));
    }

    value attributes() const
    {
        value normalize;
        normalize["axis"] = value::array{normalize_attribute::include_min};
        return {{"normalize_axes", normalize}};
    }

    std::string name() const { return "scatter_add"; }

    // reduction (pointwise operation) is called by the parent struct's compute() method.
    // It works much like a virtual function overload.
    // For the scatter methods, there are three different reduction functions.
    auto reduction() const
    {
        return [](auto& x, const auto& y) { x += y; };
    }
};

} // namespace op
} // namespace MIGRAPHX_INLINE_NS
} // namespace migraphx

#endif
