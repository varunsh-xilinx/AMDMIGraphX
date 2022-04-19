#include <migraphx/fpga/target.hpp>
#include <migraphx/fpga/lowering.hpp>
#include <migraphx/fpga/partitioning.hpp>
#include <migraphx/register_target.hpp>
#include <migraphx/pass.hpp>
#include <migraphx/auto_contiguous.hpp>
#include <migraphx/rewrite_rnn.hpp>
#include <migraphx/eliminate_pad.hpp>
#include <migraphx/insert_pad.hpp>
#include <migraphx/dead_code_elimination.hpp>
#include <migraphx/generate.hpp>
#include <migraphx/normalize_ops.hpp>

namespace migraphx {
inline namespace MIGRAPHX_INLINE_NS {
namespace fpga {

std::string target::name() const { return "fpga"; }

std::vector<pass> target::get_passes(migraphx::context& gctx, const compile_options&) const
{
    // not sure if all these passes are needed but they were copied from ref/
    auto& ctx = any_cast<context>(gctx);
    return {normalize_ops{},
            eliminate_pad{},
            dead_code_elimination{},
            insert_pad{},
            dead_code_elimination{},
            rewrite_rnn{},
            dead_code_elimination{},
            auto_contiguous{},
            dead_code_elimination{},
            partitioning{},
            dead_code_elimination{},
            lowering{&ctx},
            dead_code_elimination{}};
}

argument target::allocate(const shape& s) const { return fill_argument(s, 0); }

MIGRAPHX_REGISTER_TARGET(target);

} // namespace fpga
} // namespace MIGRAPHX_INLINE_NS
} // namespace migraphx
