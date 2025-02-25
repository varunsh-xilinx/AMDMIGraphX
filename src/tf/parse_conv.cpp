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
#include <migraphx/tf/op_parser.hpp>
#include <migraphx/ranges.hpp>
#include <migraphx/instruction.hpp>
#include <migraphx/pad_calc.hpp>
#include <migraphx/op/convolution.hpp>
#include <migraphx/make_op.hpp>

namespace migraphx {
inline namespace MIGRAPHX_INLINE_NS {
namespace tf {

struct parse_conv : op_parser<parse_conv>
{
    bool transpose() const { return true; }
    std::vector<op_desc> operators() const { return {{"Conv2D"}}; }

    instruction_ref parse(const op_desc& /*opd*/,
                          const tf_parser& parser,
                          tf_parser::node_info info,
                          std::vector<instruction_ref> args) const
    {
        op::convolution op;
        if(contains(info.attributes, "strides"))
        {
            std::vector<size_t> stride;
            copy(info.attributes.at("strides").list().i(), std::back_inserter(stride));
            parser.reorder_data(stride);
            if(stride.size() != 4)
            {
                MIGRAPHX_THROW("strides should have 4 values");
            }
            op.stride[0] = stride[2];
            op.stride[1] = stride[3];
        }
        if(contains(info.attributes, "dilations"))
        {
            std::vector<size_t> dilation;
            copy(info.attributes.at("dilations").list().i(), std::back_inserter(dilation));
            parser.reorder_data(dilation);
            if(dilation.size() != 4)
            {
                MIGRAPHX_THROW("dilation should have 4 values");
            }
            op.dilation[0] = dilation[2];
            op.dilation[1] = dilation[3];
        }

        auto weights = parser.to_kcxy(args[1]);
        auto l0      = args[0];
        if(contains(info.attributes, "padding"))
        {
            const std::string& pad_mode = info.attributes.at("padding").s();
            if(pad_mode.find("SAME") != std::string::npos)
            {
                op.padding_mode                 = op::padding_mode_t::same;
                std::vector<size_t> weight_dims = weights->get_shape().lens();
                size_t weight_h                 = weight_dims[2];
                size_t weight_w                 = weight_dims[3];

                auto input_dims = l0->get_shape().lens();
                std::vector<int64_t> pads(input_dims.size());
                calculate_padding(0, pads, input_dims[2], op.stride[0], op.dilation[0], weight_h);
                calculate_padding(1, pads, input_dims[3], op.stride[1], op.dilation[1], weight_w);

                op.padding = std::vector<size_t>(pads.begin(), pads.end());
            }
            else if(pad_mode.find("VALID") != std::string::npos)
            {
                op.padding_mode = op::padding_mode_t::valid;
            }
            else if(pad_mode.find("EXPLICIT") != std::string::npos)
            {
                std::vector<size_t> padding;
                copy(info.attributes.at("explicit_paddings").list().i(),
                     std::back_inserter(padding));
                if(padding.size() != 4)
                {
                    MIGRAPHX_THROW("padding should have 4 values");
                }
                if(padding[0] != padding[2] || padding[1] != padding[3])
                {
                    MIGRAPHX_THROW("migraphx does not support asymetric padding");
                }
                op.padding[0] = padding[0];
                op.padding[1] = padding[1];
            }
        }
        return info.add_instruction(op, {l0, weights});
    }
};

} // namespace tf
} // namespace MIGRAPHX_INLINE_NS
} // namespace migraphx
