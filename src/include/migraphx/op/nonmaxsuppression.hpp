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
#ifndef MIGRAPHX_GUARD_OPERATORS_NONMAXSUPPRESSION_HPP
#define MIGRAPHX_GUARD_OPERATORS_NONMAXSUPPRESSION_HPP

#include <cmath>
#include <queue>
#include <cstdint>
#include <iterator>
#include <migraphx/config.hpp>
#include <migraphx/ranges.hpp>
#include <migraphx/float_equal.hpp>
#include <migraphx/algorithm.hpp>
#include <migraphx/tensor_view.hpp>
#include <migraphx/shape_for_each.hpp>
#include <migraphx/check_shapes.hpp>
#include <migraphx/output_iterator.hpp>

namespace migraphx {
inline namespace MIGRAPHX_INLINE_NS {
namespace op {

struct nonmaxsuppression
{
    bool center_point_box = false;

    template <class Self, class F>
    static auto reflect(Self& self, F f)
    {
        return pack(f(self.center_point_box, "center_point_box"));
    }

    std::string name() const { return "nonmaxsuppression"; }

    shape compute_shape(std::vector<shape> inputs) const
    {
        // requires at least 2 inputs
        check_shapes{{inputs.at(0), inputs.at(1)}, *this}.only_dims(3);
        auto lens = inputs.front().lens();

        // check input shape
        if(lens[1] != inputs.at(1).lens()[2])
        {
            MIGRAPHX_THROW(
                "NonMaxSuppression: spatial dimension mismatch between boxes and scores input");
        }

        // check batch sizes
        if(lens[0] != inputs.at(1).lens()[0])
        {
            MIGRAPHX_THROW(
                "NonMaxSuppression: number of batches mismatch between boxes and scores input");
        }

        std::vector<int64_t> out_lens(2);
        out_lens.at(0) = lens.at(1);
        out_lens.at(1) = 3;
        return {shape::int64_type, out_lens};
    }

    struct box
    {
        std::array<double, 2> x;
        std::array<double, 2> y;

        void sort()
        {
            std::sort(x.begin(), x.end());
            std::sort(y.begin(), y.end());
        }

        std::array<double, 2>& operator[](std::size_t i) { return i == 0 ? x : y; }

        double area() const
        {
            assert(std::is_sorted(x.begin(), x.end()));
            assert(std::is_sorted(y.begin(), y.end()));
            return (x[1] - x[0]) * (y[1] - y[0]);
        }
    };

    template <class T>
    box batch_box(T boxes, std::size_t box_idx) const
    {
        box result{};
        auto start = boxes + 4 * box_idx;
        if(center_point_box)
        {
            double half_width  = start[2] / 2.0;
            double half_height = start[3] / 2.0;
            double x_center    = start[0];
            double y_center    = start[1];
            result.x           = {x_center - half_width, x_center + half_width};
            result.y           = {y_center - half_height, y_center + half_height};
        }
        else
        {
            result.x = {static_cast<double>(start[1]), static_cast<double>(start[3])};
            result.y = {static_cast<double>(start[0]), static_cast<double>(start[2])};
        }

        return result;
    }

    inline bool suppress_by_iou(box b1, box b2, double iou_threshold) const
    {
        b1.sort();
        b2.sort();

        box intersection{};
        for(auto i : range(2))
        {
            intersection[i][0] = std::max(b1[i][0], b2[i][0]);
            intersection[i][1] = std::min(b1[i][1], b2[i][1]);
        }

        std::vector<std::array<double, 2>> bbox = {intersection.x, intersection.y};
        if(std::any_of(bbox.begin(), bbox.end(), [](auto bx) {
               return not std::is_sorted(bx.begin(), bx.end());
           }))
        {
            return false;
        }

        const double area1             = b1.area();
        const double area2             = b2.area();
        const double intersection_area = intersection.area();
        const double union_area        = area1 + area2 - intersection_area;

        if(area1 <= .0f or area2 <= .0f or union_area <= .0f)
        {
            return false;
        }

        const double intersection_over_union = intersection_area / union_area;

        return intersection_over_union > iou_threshold;
    }

    // filter boxes below score_threshold
    template <class T>
    std::priority_queue<std::pair<double, int64_t>>
    filter_boxes_by_score(T scores_start, std::size_t num_boxes, double score_threshold) const
    {
        std::priority_queue<std::pair<double, int64_t>> boxes_heap;
        auto insert_to_boxes_heap =
            make_function_output_iterator([&](const auto& x) { boxes_heap.push(x); });
        int64_t box_idx = 0;
        transform_if(
            scores_start,
            scores_start + num_boxes,
            insert_to_boxes_heap,
            [&](auto sc) {
                box_idx++;
                return sc >= score_threshold;
            },
            [&](auto sc) { return std::make_pair(sc, box_idx - 1); });
        return boxes_heap;
    }

    template <class Output, class Boxes, class Scores>
    void compute_nms(Output output,
                     Boxes boxes,
                     Scores scores,
                     const shape& output_shape,
                     std::size_t max_output_boxes_per_class,
                     double iou_threshold,
                     double score_threshold) const
    {
        std::fill(output.begin(), output.end(), 0);
        const auto& lens       = scores.get_shape().lens();
        const auto num_batches = lens[0];
        const auto num_classes = lens[1];
        const auto num_boxes   = lens[2];
        // boxes of a class with NMS applied [score, index]
        std::vector<std::pair<double, int64_t>> selected_boxes_inside_class;
        std::vector<int64_t> selected_indices;
        selected_boxes_inside_class.reserve(output_shape.elements());
        // iterate over batches and classes
        shape comp_s{shape::double_type, {num_batches, num_classes}};
        shape_for_each(comp_s, [&](auto idx) {
            auto batch_idx = idx[0];
            auto class_idx = idx[1];
            // index offset for this class
            auto scores_start = scores.begin() + (batch_idx * num_classes + class_idx) * num_boxes;
            // iterator to first value of this batch
            auto batch_boxes_start = boxes.begin() + batch_idx * num_boxes * 4;
            auto boxes_heap = filter_boxes_by_score(scores_start, num_boxes, score_threshold);
            selected_boxes_inside_class.clear();
            // Get the next box with top score, filter by iou_threshold
            while(!boxes_heap.empty() &&
                  selected_boxes_inside_class.size() < max_output_boxes_per_class)
            {
                // Check with existing selected boxes for this class, remove box if it
                // exceeds the IOU (Intersection Over Union) threshold
                const auto next_top_score = boxes_heap.top();
                bool not_selected =
                    std::any_of(selected_boxes_inside_class.begin(),
                                selected_boxes_inside_class.end(),
                                [&](auto selected_index) {
                                    return this->suppress_by_iou(
                                        batch_box(batch_boxes_start, next_top_score.second),
                                        batch_box(batch_boxes_start, selected_index.second),
                                        iou_threshold);
                                });

                if(not not_selected)
                {
                    selected_boxes_inside_class.push_back(next_top_score);
                    selected_indices.push_back(batch_idx);
                    selected_indices.push_back(class_idx);
                    selected_indices.push_back(next_top_score.second);
                }
                boxes_heap.pop();
            }
        });
        std::copy(selected_indices.begin(), selected_indices.end(), output.begin());
    }

    argument compute(const shape& output_shape, std::vector<argument> args) const
    {
        argument result{output_shape};

        std::size_t max_output_boxes_per_class =
            (args.size() > 2) ? (args.at(2).at<std::size_t>()) : 0;
        if(max_output_boxes_per_class == 0)
        {
            return result;
        }
        double iou_threshold   = (args.size() > 3) ? (args.at(3).at<double>()) : 0.0f;
        double score_threshold = (args.size() > 4) ? (args.at(4).at<double>()) : 0.0f;

        result.visit([&](auto output) {
            visit_all(args[0], args[1])([&](auto boxes, auto scores) {
                compute_nms(output,
                            boxes,
                            scores,
                            output_shape,
                            max_output_boxes_per_class,
                            iou_threshold,
                            score_threshold);
            });
        });

        return result;
    }
};

} // namespace op
} // namespace MIGRAPHX_INLINE_NS
} // namespace migraphx

#endif
