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
#include <migraphx/gpu/leaky_relu.hpp>
#include <migraphx/gpu/context.hpp>
#include <migraphx/gpu/miopen.hpp>

namespace migraphx {
inline namespace MIGRAPHX_INLINE_NS {
namespace gpu {

shape miopen_leaky_relu::compute_shape(const std::vector<shape>& inputs) const
{
    check_shapes{inputs, *this}.has(2).not_broadcasted();
    return inputs.at(1);
}

argument miopen_leaky_relu::compute(context& ctx,
                                    const shape& output_shape,
                                    const std::vector<argument>& args) const
{
    float alpha = 1;
    float beta  = 0;
    auto x_desc = make_tensor(args[0].get_shape());
    auto y_desc = make_tensor(output_shape);
    miopenActivationForward(ctx.get_stream().get_miopen(),
                            ad.get(),
                            &alpha,
                            x_desc.get(),
                            args[0].implicit(),
                            &beta,
                            y_desc.get(),
                            args[1].implicit());

    return args[1];
}

void miopen_leaky_relu::finalize(context&, const shape&, const std::vector<shape>&)
{
    ad = make_leaky_relu(op.alpha);
}

} // namespace gpu
} // namespace MIGRAPHX_INLINE_NS
} // namespace migraphx
