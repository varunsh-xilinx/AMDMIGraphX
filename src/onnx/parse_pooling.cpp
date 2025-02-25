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
#include <migraphx/onnx/op_parser.hpp>
#include <migraphx/onnx/checks.hpp>
#include <migraphx/onnx/padding.hpp>
#include <migraphx/op/pad.hpp>
#include <migraphx/op/pooling.hpp>
#include <migraphx/instruction.hpp>
#include <migraphx/ranges.hpp>
#include <migraphx/stringutils.hpp>
#include <migraphx/make_op.hpp>

namespace migraphx {
inline namespace MIGRAPHX_INLINE_NS {
namespace onnx {

struct parse_pooling : op_parser<parse_pooling>
{
    std::vector<op_desc> operators() const
    {
        return {{"AveragePool", "average"},
                {"GlobalAveragePool", "average"},
                {"GlobalMaxPool", "max"},
                {"MaxPool", "max"},
                {"LpPool", "lpnorm"},
                {"GlobalLpPool", "lpnorm"}};
    }

    instruction_ref parse(const op_desc& opd,
                          const onnx_parser& /*parser*/,
                          onnx_parser::node_info info,
                          std::vector<instruction_ref> args) const
    {
        const std::unordered_map<std::string, op::pooling_mode> mode_map = {
            {"max", op::pooling_mode::max},
            {"average", op::pooling_mode::average},
            {"lpnorm", op::pooling_mode::lpnorm}};
        std::string mode = opd.op_name;
        if(not contains(mode_map, mode))
        {
            MIGRAPHX_THROW("onnx pooling mode must be [\"max\", \"average\", \"lpnorm\"]");
        }
        operation op = make_op("pooling", {{"mode", mode_map.at(mode)}});
        value values = op.to_value();
        auto l0      = args[0];
        auto in_lens = l0->get_shape().lens();
        assert(in_lens.size() > 2);
        auto kdims = in_lens.size() - 2;

        if(starts_with(opd.onnx_name, "Global"))
        {
            values["lengths"] = std::vector<size_t>(in_lens.begin() + 2, in_lens.end());
        }

        // does not support ceil_mode
        if(contains(info.attributes, "ceil_mode"))
        {
            values["ceil_mode"] = static_cast<bool>(info.attributes.at("ceil_mode").i());
        }

        // count include padding, if count include pad is 1, we always use
        // explicit pad
        int count_include_pad = 0;
        if(contains(info.attributes, "count_include_pad"))
        {
            count_include_pad = info.attributes.at("count_include_pad").i();
        }

        if(contains(info.attributes, "strides"))
        {
            values["stride"].clear();
            copy(info.attributes["strides"].ints(), std::back_inserter(values["stride"]));
            check_attr_sizes(kdims, values["stride"].size(), "PARSE_POOLING: inconsistent strides");
        }
        if(contains(info.attributes, "kernel_shape"))
        {
            values["lengths"].clear();
            copy(info.attributes["kernel_shape"].ints(), std::back_inserter(values["lengths"]));
            check_attr_sizes(
                kdims, values["lengths"].size(), "PARSE_POOLING: inconsistent lengths");
        }

        // lp_order attribute
        if(contains(info.attributes, "p"))
        {
            values["lp_order"] = info.attributes.at("p").i();
        }

        // ensure pads availabe only when auto_pad is "NOT_SET"
        check_padding_mode(info, "POOLING");

        std::vector<int64_t> paddings;
        float pad_val = ((mode == "max") ? std::numeric_limits<float>::lowest() : 0.0f);

        if(contains(info.attributes, "pads"))
        {
            values["padding"].clear();
            copy(info.attributes["pads"].ints(), std::back_inserter(paddings));
            check_attr_sizes(
                kdims, paddings.size() / 2, "PARSE_POOLING: inconsistent explicit paddings");
        }

        if(contains(info.attributes, "auto_pad"))
        {
            values["padding"].clear();
            // return paddings could be empty, then setting to 0 for no padding
            cal_auto_padding_size(info,
                                  values,
                                  values["lengths"].to_vector<std::size_t>(),
                                  {1, 1},
                                  in_lens,
                                  paddings);
        }

        if(paddings.size() != 2 * kdims)
        {
            paddings.resize(kdims * 2);
            std::fill_n(paddings.begin(), 2 * kdims, 0);
        }

        if(values["padding"].size() != kdims)
        {
            values["padding"].resize(kdims);
            std::fill_n(values["padding"].begin(), kdims, 0);
        }

        if(values["stride"].size() != kdims)
        {
            values["stride"].resize(kdims);
            std::fill_n(values["stride"].begin(), kdims, 1);
        }
        // used to calculate the supposed output shape
        std::vector<int64_t> orig_padding = paddings;

        std::vector<int64_t> slice_start;
        std::vector<int64_t> slice_end;
        tune_padding_size(values, paddings, count_include_pad, slice_start);

        if(!slice_start.empty())
        {
            // calculate expected output shape
            orig_padding.insert(orig_padding.begin() + kdims, 2, 0);
            orig_padding.insert(orig_padding.begin(), 2, 0);
            op::pad pad{orig_padding, 0.0f};
            shape padded_shape = pad.compute_shape({l0->get_shape()});
            auto out_lens      = make_op("pooling", values).compute_shape({padded_shape}).lens();

            // compute slice_end information
            slice_end.resize(slice_start.size());
            std::transform(out_lens.begin() + 2,
                           out_lens.end(),
                           slice_start.begin(),
                           slice_end.begin(),
                           [](auto i, auto j) { return i + j; });
        }
        values["padding"] = std::vector<size_t>(paddings.begin(), paddings.end());

        check_asym_padding(info, l0, paddings, values, count_include_pad, pad_val);
        op.from_value(values);

        auto l1 = info.add_instruction(op, l0);
        if(!slice_start.empty())
        {
            std::vector<int64_t> axes(kdims);
            std::iota(axes.begin(), axes.end(), 2);
            l1 = info.add_instruction(
                make_op("slice", {{"axes", axes}, {"starts", slice_start}, {"ends", slice_end}}),
                l1);
        }

        return l1;
    }
};

} // namespace onnx
} // namespace MIGRAPHX_INLINE_NS
} // namespace migraphx
