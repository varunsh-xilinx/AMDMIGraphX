#ifndef MIGRAPHX_GUARD_AMDMIGRAPHX_KERNELS_TYPE_TRAITS_HPP
#define MIGRAPHX_GUARD_AMDMIGRAPHX_KERNELS_TYPE_TRAITS_HPP

#include <migraphx/kernels/types.hpp>
#include <migraphx/kernels/integral_constant.hpp>

namespace migraphx {

template <class T>
struct type_identity
{
    using type = T;
};

template <bool B, class T = void>
struct enable_if
{
};

template <class T>
struct enable_if<true, T>
{
    using type = T;
};

template <bool B, class T = void>
using enable_if_t = typename enable_if<B, T>::type;

#define MIGRAPHX_BUILTIN_TYPE_TRAIT1(name) \
template <class T> \
struct name : bool_constant<__ ## name(T)> {}

#define MIGRAPHX_BUILTIN_TYPE_TRAIT2(name) \
template <class T, class U> \
struct name : bool_constant<__ ## name(T, U)> {}

#define MIGRAPHX_BUILTIN_TYPE_TRAITN(name) \
template <class... Ts> \
struct name : bool_constant<__ ## name(Ts...)> {}

// MIGRAPHX_BUILTIN_TYPE_TRAIT1(is_arithmetic);
// MIGRAPHX_BUILTIN_TYPE_TRAIT1(is_destructible);
// MIGRAPHX_BUILTIN_TYPE_TRAIT1(is_nothrow_destructible);
// MIGRAPHX_BUILTIN_TYPE_TRAIT1(is_pointer);
// MIGRAPHX_BUILTIN_TYPE_TRAIT1(is_scalar);
// MIGRAPHX_BUILTIN_TYPE_TRAIT1(is_signed);
// MIGRAPHX_BUILTIN_TYPE_TRAIT1(is_void);
MIGRAPHX_BUILTIN_TYPE_TRAIT1(is_abstract);
MIGRAPHX_BUILTIN_TYPE_TRAIT1(is_aggregate);
MIGRAPHX_BUILTIN_TYPE_TRAIT1(is_array);
MIGRAPHX_BUILTIN_TYPE_TRAIT1(is_class);
MIGRAPHX_BUILTIN_TYPE_TRAIT1(is_compound);
MIGRAPHX_BUILTIN_TYPE_TRAIT1(is_const);
MIGRAPHX_BUILTIN_TYPE_TRAIT1(is_empty);
MIGRAPHX_BUILTIN_TYPE_TRAIT1(is_enum);
MIGRAPHX_BUILTIN_TYPE_TRAIT1(is_final);
MIGRAPHX_BUILTIN_TYPE_TRAIT1(is_floating_point);
MIGRAPHX_BUILTIN_TYPE_TRAIT1(is_function);
MIGRAPHX_BUILTIN_TYPE_TRAIT1(is_fundamental);
MIGRAPHX_BUILTIN_TYPE_TRAIT1(is_integral);
MIGRAPHX_BUILTIN_TYPE_TRAIT1(is_literal_type);
MIGRAPHX_BUILTIN_TYPE_TRAIT1(is_lvalue_reference);
MIGRAPHX_BUILTIN_TYPE_TRAIT1(is_member_function_pointer);
MIGRAPHX_BUILTIN_TYPE_TRAIT1(is_member_object_pointer);
MIGRAPHX_BUILTIN_TYPE_TRAIT1(is_member_pointer);
MIGRAPHX_BUILTIN_TYPE_TRAIT1(is_object);
MIGRAPHX_BUILTIN_TYPE_TRAIT1(is_pod);
MIGRAPHX_BUILTIN_TYPE_TRAIT1(is_polymorphic);
MIGRAPHX_BUILTIN_TYPE_TRAIT1(is_reference);
MIGRAPHX_BUILTIN_TYPE_TRAIT1(is_rvalue_reference);
MIGRAPHX_BUILTIN_TYPE_TRAIT1(is_standard_layout);
MIGRAPHX_BUILTIN_TYPE_TRAIT1(is_trivial);
MIGRAPHX_BUILTIN_TYPE_TRAIT1(is_trivially_copyable);
MIGRAPHX_BUILTIN_TYPE_TRAIT1(is_trivially_destructible);
MIGRAPHX_BUILTIN_TYPE_TRAIT1(is_union);
MIGRAPHX_BUILTIN_TYPE_TRAIT1(is_unsigned);
MIGRAPHX_BUILTIN_TYPE_TRAIT1(is_volatile);
MIGRAPHX_BUILTIN_TYPE_TRAIT2(is_assignable);
MIGRAPHX_BUILTIN_TYPE_TRAIT2(is_base_of);
MIGRAPHX_BUILTIN_TYPE_TRAIT2(is_convertible);
MIGRAPHX_BUILTIN_TYPE_TRAIT2(is_nothrow_assignable);
MIGRAPHX_BUILTIN_TYPE_TRAIT2(is_same);
MIGRAPHX_BUILTIN_TYPE_TRAIT2(is_trivially_assignable);
MIGRAPHX_BUILTIN_TYPE_TRAITN(is_constructible);
MIGRAPHX_BUILTIN_TYPE_TRAITN(is_nothrow_constructible);
MIGRAPHX_BUILTIN_TYPE_TRAITN(is_trivially_constructible);

template <class T>
struct remove_reference
{
    using type = T;
};
template <class T>
struct remove_reference<T&>
{
    using type = T;
};
template <class T>
struct remove_reference<T&&>
{
    using type = T;
};

template <class T>
using remove_reference_t = typename remove_reference<T>::type;

template <class T>
struct add_pointer : type_identity<typename remove_reference<T>::type*>
{
};

template <class T>
using add_pointer_t = typename add_pointer<T>::type;

constexpr unsigned long int_max(unsigned long n)
{
    return (1 << (n*8)) - 1;
}

template<class T>
constexpr T numeric_max()
{
    if constexpr(is_integral<T>{})
    {
        if constexpr(is_unsigned<T>{})
            return int_max(sizeof(T)) * 2;
        else
            return int_max(sizeof(T));
    }
    else if constexpr(is_same<T, double>{})
        return __DBL_MAX__;
    else if constexpr(is_same<T, float>{})
        return __FLT_MAX__;
    else if constexpr(is_same<T, migraphx::half>{})
        return __FLT16_MAX__;
    else
        return 0;
}

template<class T>
constexpr T numeric_lowest()
{
    if constexpr(is_integral<T>{})
    {
        if constexpr(is_unsigned<T>{})
            return 0;
        else
            return -numeric_max<T>() - 1;
    }
    else
    {
        return -numeric_max<T>();
    }
}

#define MIGRAPHX_REQUIRES(...) class = enable_if_t<__VA_ARGS__>

} // namespace migraphx

#endif
