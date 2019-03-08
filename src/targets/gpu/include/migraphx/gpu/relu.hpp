#ifndef MIGRAPHX_GUARD_RTGLIB_RELU_HPP
#define MIGRAPHX_GUARD_RTGLIB_RELU_HPP

#include <migraphx/shape.hpp>
#include <migraphx/gpu/miopen.hpp>

namespace migraphx {
inline namespace MIGRAPHX_INLINE_NS {
namespace gpu {

struct context;

struct miopen_relu
{
    shared<activation_descriptor> ad;
    std::string name() const { return "gpu::relu"; }
    shape compute_shape(const std::vector<shape>& inputs) const;
    argument
    compute(context& ctx, const shape& output_shape, const std::vector<argument>& args) const;
    int output_alias(const std::vector<shape>& shapes) const { return shapes.size() - 1; }
};

} // namespace gpu
} // namespace MIGRAPHX_INLINE_NS
} // namespace migraphx

#endif
