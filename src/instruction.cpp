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
#include <migraphx/instruction.hpp>
#include <migraphx/builtin.hpp>
#include <migraphx/erase.hpp>
#include <migraphx/module.hpp>
#include <migraphx/ranges.hpp>

namespace migraphx {
inline namespace MIGRAPHX_INLINE_NS {

template <class T>
auto equal_to(const T& x)
{
    return [&](const T& y) { return std::equal_to<T>{}(x, y); };
}

instruction::instruction(operation o, shape r, std::vector<instruction_ref> args)
    : op(std::move(o)), result(std::move(r)), arguments(std::move(args))
{
}

instruction::instruction(operation o,
                         shape r,
                         std::vector<instruction_ref> args,
                         std::vector<module_ref> modules)
    : op(std::move(o)),
      result(std::move(r)),
      arguments(std::move(args)),
      module_args(std::move(modules))
{
}

instruction::instruction(literal l)
    : op(builtin::literal{}), result(l.get_shape()), lit(std::move(l))
{
}

void instruction::replace(const shape& r)
{
    if(r != result)
    {
        result = r;
        for(auto&& ins : output)
        {
            if(ins->name() == "@return")
                continue;

            assert(ins->name().front() != '@');
            ins->recompute_shape();
        }
    }
}

void instruction::replace(operation o)
{
    normalized = false;
    op         = std::move(o);
    recompute_shape();
}

void instruction::recompute_shape() { replace(compute_shape(op, arguments, module_args)); }

void instruction::clear_arguments()
{
    for(auto&& arg : arguments)
    {
        arg->remove_output(*this);
    }
    arguments.clear();
    module_args.clear();
}

bool operator==(const instruction& i, instruction_ref ref)
{
    return std::addressof(i) == std::addressof(*ref);
}

bool instruction::valid(instruction_ref start, bool check_order) const
{
    return valid() && std::all_of(arguments.begin(), arguments.end(), [&](instruction_ref i) {
               auto self = std::find(i->outputs().begin(), i->outputs().end(), *this);
               bool ret  = self != i->outputs().end();
               if(check_order)
               {
                   // check arguments for this instruction before this instruction
                   ret = ret and (std::distance(start, i) < std::distance(start, *self));
               }
               return ret;
           });
}

bool instruction::valid() const
{
    shape computed;
    if(op.name() == "@literal")
    {
        computed = lit.get_shape();
    }
    else if(op.name() == "@param")
    {
        computed = result;
    }
    else if(op.name() == "@return")
    {
        computed = {};
    }
    else
    {
        try
        {
            computed = compute_shape(op, arguments, module_args);
        }
        catch(migraphx::exception&)
        {
            return false;
        }
    }

    return (result == computed) &&
           std::all_of(output.begin(), output.end(), [&](instruction_ref i) {
               return std::find(i->inputs().begin(), i->inputs().end(), *this) != i->inputs().end();
           });
}

shape instruction::get_shape() const { return result; }
const literal& instruction::get_literal() const
{
    assert(op.name() == "@literal");
    return lit;
}

const operation& instruction::get_operator() const { return op; }

std::string instruction::name() const { return op.name(); }

const std::vector<instruction_ref>& instruction::inputs() const { return arguments; }

const std::vector<module_ref>& instruction::module_inputs() const { return module_args; }

const std::vector<instruction_ref>& instruction::outputs() const { return output; }

bool operator==(const instruction& x, const instruction& y)
{
    if(not std::equal(x.arguments.begin(),
                      x.arguments.end(),
                      y.arguments.begin(),
                      y.arguments.end(),
                      std::equal_to<instruction_ref>{}))
        return false;
    if(std::tie(x.result, x.op, x.module_args) != std::tie(y.result, y.op, y.module_args))
        return false;
    if(x.name() == "@literal")
        return x.lit == y.lit;
    return true;
}

bool operator!=(const instruction& x, const instruction& y) { return !(x == y); }

bool operator==(instruction_ref ref, const instruction& i) { return i == ref; }

bool operator!=(const instruction& i, instruction_ref ref) { return !(i == ref); }

bool operator!=(instruction_ref ref, const instruction& i) { return !(i == ref); }

void instruction::add_output(instruction_ref ins)
{
    if(std::find_if(output.begin(), output.end(), equal_to(ins)) == output.end())
        output.push_back(ins);
}

void instruction::backreference(instruction_ref ref)
{
    for(auto&& arg : ref->inputs())
        arg->add_output(ref);
}

void instruction::replace_argument(instruction_ref ins,
                                   instruction_ref old,
                                   instruction_ref new_ins)
{
    ins->replace_argument(old, new_ins);
    backreference(ins);
    ins->recompute_shape();
}

void instruction::replace_mod_argument(instruction_ref ins, module_ref old, module_ref new_mod)
{
    ins->replace_mod_argument(old, new_mod);
    backreference(ins);
    ins->recompute_shape();
}

void instruction::replace(instruction_ref ins,
                          operation o,
                          const shape& r,
                          std::vector<instruction_ref> args)
{
    ins->replace(std::move(o), r, std::move(args));
    backreference(ins);
}

void instruction::replace(instruction_ref ins,
                          operation o,
                          const shape& r,
                          std::vector<instruction_ref> args,
                          std::vector<module_ref> module_args)
{
    ins->replace(std::move(o), r, std::move(args), std::move(module_args));
    backreference(ins);
}

void instruction::replace(operation o, const shape& r, std::vector<instruction_ref> args)
{
    normalized = false;
    op         = std::move(o);
    replace(r);
    replace(std::move(args));
}

void instruction::replace(operation o,
                          const shape& r,
                          std::vector<instruction_ref> args,
                          std::vector<module_ref> mdl_args)
{
    op = std::move(o);
    replace(r);
    replace(std::move(args), std::move(mdl_args));
}

void instruction::replace_refs(
    instruction_ref ins,
    const std::unordered_map<instruction_ref, instruction_ref>& map_insts,
    const std::unordered_map<module_ref, module_ref>& map_mods)
{
    const auto& args = ins->inputs();
    for(const auto& arg : args)
    {
        if(contains(map_insts, arg))
        {
            instruction::replace_argument(ins, arg, map_insts.at(arg));
        }
    }

    const auto& module_args = ins->module_inputs();
    if(module_args.empty())
        return;

    for(const auto& mod : module_args)
    {
        if(contains(map_mods, mod))
        {
            instruction::replace_mod_argument(ins, mod, map_mods.at(mod));
        }
    }
}

void instruction::replace(std::vector<instruction_ref> args)
{
    clear_arguments();
    arguments = std::move(args);
}

void instruction::replace(std::vector<instruction_ref> args, std::vector<module_ref> mdl_args)
{
    clear_arguments();
    arguments   = std::move(args);
    module_args = std::move(mdl_args);
}

void instruction::replace_argument(instruction_ref old, instruction_ref new_ins)
{
    assert(std::any_of(arguments.begin(), arguments.end(), equal_to(old)));
    std::replace_if(arguments.begin(), arguments.end(), equal_to(old), new_ins);
    old->remove_output(*this);
}

void instruction::replace_mod_argument(module_ref old, module_ref new_mod)
{
    assert(std::any_of(module_args.begin(), module_args.end(), [&](auto i) { return i == old; }));
    std::replace(module_args.begin(), module_args.end(), old, new_mod);
}

bool instruction::can_eval() const
{
    if(op.name() == "@literal")
    {
        return true;
    }
    else if(is_context_free(op))
    {
        return std::all_of(
            this->inputs().begin(), this->inputs().end(), [](auto arg) { return arg->can_eval(); });
    }
    else
    {
        return false;
    }
}

argument instruction::eval(bool check_eval) const
{
    if(op.name() == "@literal")
    {
        return this->get_literal().get_argument();
    }
    if(is_context_free(op))
    {
        if(check_eval and not this->can_eval())
            return {};
        std::vector<argument> args;
        std::transform(this->inputs().begin(),
                       this->inputs().end(),
                       std::back_inserter(args),
                       [](auto arg) { return arg->eval(false); });
        return normalized_operator().compute(result, args);
    }
    return {};
}

void instruction::finalize(context& ctx)
{
    if(has_finalize(this->op))
        this->op.finalize(ctx, this->get_shape(), to_shapes(this->inputs()));
}

void instruction::print(std::ostream& os,
                        instruction_ref ins,
                        const std::unordered_map<instruction_ref, std::string>& names)
{
    os << names.at(ins) << " = ";

    os << ins->get_operator();

    if(ins->name() == "@literal")
    {
        if(ins->get_literal().get_shape().elements() > 10)
            os << "{ ... }";
        else
            os << "{" << ins->get_literal() << "}";
    }

    if(!ins->inputs().empty())
    {
        char delim = '(';
        for(auto&& arg : ins->inputs())
        {
            std::string arg_name = contains(names, arg) ? names.at(arg) : "?";
            os << delim << arg_name;
            delim = ',';
        }
        os << ")";
    }

    // print module inputs
    if(!ins->module_inputs().empty())
    {
        std::string delim = ", [";
        for(auto&& mod_arg : ins->module_inputs())
        {
            os << delim << mod_arg->name();
            delim = ", ";
        }
        os << "]";
    }

    // skip return instruction shape
    if(ins->name() != "@return")
        os << " -> " << ins->get_shape();
}

static void debug_name(std::ostream& os, const instruction& ins)
{
    if(ins.name() == "@literal")
    {
        os << "@literal";
        if(ins.get_literal().get_shape().elements() > 10)
            os << "{ ... }";
        else
            os << "{" << ins.get_literal() << "}";
    }
    else
    {
        os << ins.get_operator();
    }
}

void instruction::debug_print() const
{
    debug_name(std::cout, *this);
    std::string delim = "(";
    for(auto arg : this->inputs())
    {
        std::cout << delim;
        debug_name(std::cout, *arg);
        delim = ", ";
    }
    if(not this->inputs().empty())
        std::cout << ")";
    std::cout << " -> " << this->get_shape() << std::endl;
}

instruction_ref instruction::get_output_alias(instruction_ref ins, bool shallow)
{
    auto i = ins->get_operator().output_alias(to_shapes(ins->inputs()));
    if(i < 0)
        return ins;
    if(shallow)
        return ins->inputs().at(i);
    return get_output_alias(ins->inputs().at(i));
}

void instruction::set_normalized(bool value) { normalized = value; }

bool instruction::is_normalized() const { return normalized; }

bool instruction::need_normalization() const
{
    return this->get_operator().need_normalization() and not normalized;
}

operation instruction::normalized_operator() const
{
    operation o = this->get_operator();
    if(this->need_normalization())
    {
        auto lens = this->inputs().front()->get_shape().lens();
        if(!normalize_attributes(o, lens))
            return this->get_operator();
    }
    return o;
}

std::vector<shape> to_shapes(const std::vector<instruction_ref>& args)
{
    std::vector<shape> shapes(args.size());
    std::transform(
        args.begin(), args.end(), shapes.begin(), [](instruction_ref i) { return i->get_shape(); });
    return shapes;
}

shape compute_shape(const operation& op, const std::vector<instruction_ref>& args)
{
    return op.compute_shape(to_shapes(args));
}

shape compute_shape(const operation& op,
                    const std::vector<instruction_ref>& args,
                    const std::vector<module_ref>& mods)
{
    if(mods.empty())
    {
        return op.compute_shape(to_shapes(args));
    }
    else
    {
        return op.compute_shape(to_shapes(args), mods);
    }
}

std::vector<shape> try_compute_shape(const operation& op, const std::vector<shape>& inputs)
{
    shape new_shape;
    try
    {
        new_shape = op.compute_shape(inputs);
    }
    catch(...)
    {
        return {};
    }
    return {new_shape};
}

migraphx::instruction* as_address(const instruction_ref& ins) noexcept
{
    return std::addressof(*ins);
}

} // namespace MIGRAPHX_INLINE_NS
} // namespace migraphx
