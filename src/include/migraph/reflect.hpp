#ifndef MIGRAPH_GUARD_RTGLIB_REFLECT_HPP
#define MIGRAPH_GUARD_RTGLIB_REFLECT_HPP

#include <migraph/functional.hpp>
#include <migraph/rank.hpp>
#include <functional>

namespace migraph {

namespace detail {

template <class T, class Selector>
auto reflect_impl(rank<1>, T& x, Selector f) -> decltype(T::reflect(x, f))
{
    return T::reflect(x, std::move(f));
}

template <class T, class Selector>
auto reflect_impl(rank<0>, T&, Selector)
{
    return pack();
}

} // namespace detail

template <class T, class Selector>
auto reflect(T& x, Selector f)
{
    return detail::reflect_impl(rank<1>{}, x, std::move(f));
}

template <class T>
auto reflect_tie(T& x)
{
    return reflect(x, [](auto&& y, auto&&...) { return std::ref(y); })(
        [](auto&&... xs) { return std::tie(xs.get()...); });
}

template <class T, class F>
void reflect_each(T& x, F f)
{
    return reflect(x, [](auto&& y, auto... ys) { return pack(std::ref(y), ys...); })(
        [&](auto&&... xs) {
            each_args([&](auto p) { p([&](auto&& y, auto... ys) { f(y, ys...); }); }, xs...);
        });
}

} // namespace migraph

#endif
