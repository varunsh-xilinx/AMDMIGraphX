
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <sstream>
#include <unordered_map>
#include <vector>

#ifdef __linux__
#include <sys/types.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#ifndef MIGRAPHX_GUARD_TEST_TEST_HPP
#define MIGRAPHX_GUARD_TEST_TEST_HPP

#if defined(__has_feature)
#if __has_feature(address_sanitizer)
// NOLINTNEXTLINE
#define TEST_SANITIZE_ADDRESS 1
#endif
#endif

#if defined(__SANITIZE_ADDRESS__)
#if __SANITIZE_ADDRESS__
// NOLINTNEXTLINE
#define TEST_SANITIZE_ADDRESS 1
#endif
#endif

#ifndef TEST_SANITIZE_ADDRESS
// NOLINTNEXTLINE
#define TEST_SANITIZE_ADDRESS 0
#endif

namespace test {
// clang-format off
// NOLINTNEXTLINE
#define TEST_FOREACH_BINARY_OPERATORS(m) \
    m(==, equal) \
    m(!=, not_equal) \
    m(<=, less_than_equal) \
    m(>=, greater_than_equal) \
    m(<, less_than) \
    m(>, greater_than) \
    m(and, and_op) \
    m(or, or_op)
// clang-format on

// clang-format off
// NOLINTNEXTLINE
#define TEST_FOREACH_UNARY_OPERATORS(m) \
    m(not, not_op)
// clang-format on

// NOLINTNEXTLINE
#define TEST_EACH_BINARY_OPERATOR_OBJECT(op, name)     \
    struct name                                        \
    {                                                  \
        static std::string as_string() { return #op; } \
        template <class T, class U>                    \
        static decltype(auto) call(T&& x, U&& y)       \
        {                                              \
            return x op y;                             \
        }                                              \
    };

// NOLINTNEXTLINE
#define TEST_EACH_UNARY_OPERATOR_OBJECT(op, name)      \
    struct name                                        \
    {                                                  \
        static std::string as_string() { return #op; } \
        template <class T>                             \
        static decltype(auto) call(T&& x)              \
        {                                              \
            return op x;                               \
        }                                              \
    };

TEST_FOREACH_BINARY_OPERATORS(TEST_EACH_BINARY_OPERATOR_OBJECT)
TEST_FOREACH_UNARY_OPERATORS(TEST_EACH_UNARY_OPERATOR_OBJECT)

struct nop
{
    static std::string as_string() { return ""; }
    template <class T>
    static decltype(auto) call(T&& x)
    {
        return x;
    }
};

struct function
{
    static std::string as_string() { return ""; }
    template <class T>
    static decltype(auto) call(T&& x)
    {
        return x();
    }
};

template <class Iterator>
inline std::ostream& stream_range(std::ostream& s, Iterator start, Iterator last)
{
    if(start != last)
    {
        s << *start;
        std::for_each(std::next(start), last, [&](auto&& x) { s << ", " << x; });
    }
    return s;
}

inline std::ostream& operator<<(std::ostream& s, std::nullptr_t)
{
    s << "nullptr";
    return s;
}

template <class T>
inline std::ostream& operator<<(std::ostream& s, const std::vector<T>& v)
{
    s << "{ ";
    stream_range(s, v.begin(), v.end());
    s << "}";
    return s;
}

inline std::ostream& operator<<(std::ostream& s, const std::vector<bool>& v)
{
    s << "{ ";
    stream_range(s, v.begin(), v.end());
    s << "}";
    return s;
}

template <class T, class U, class Operator>
struct expression
{
    T lhs;
    U rhs;

    friend std::ostream& operator<<(std::ostream& s, const expression& self)
    {
        s << self.lhs << " " << Operator::as_string() << " " << self.rhs;
        return s;
    }

    decltype(auto) value() const { return Operator::call(lhs, rhs); };
};

// TODO: Remove rvalue references
template <class T, class U, class Operator>
expression<T, U, Operator> make_expression(T&& rhs, U&& lhs, Operator)
{
    return {std::forward<T>(rhs), std::forward<U>(lhs)};
}

template <class T, class Operator = nop>
struct lhs_expression;

// TODO: Remove rvalue reference
template <class T>
lhs_expression<T> make_lhs_expression(T&& lhs)
{
    return lhs_expression<T>{std::forward<T>(lhs)};
}

template <class T, class Operator>
lhs_expression<T, Operator> make_lhs_expression(T&& lhs, Operator)
{
    return lhs_expression<T, Operator>{std::forward<T>(lhs)};
}

template <class T, class Operator>
struct lhs_expression
{
    T lhs;
    explicit lhs_expression(T e) : lhs(e) {}

    friend std::ostream& operator<<(std::ostream& s, const lhs_expression& self)
    {
        std::string op = Operator::as_string();
        if(not op.empty())
            s << Operator::as_string() << " ";
        s << self.lhs;
        return s;
    }

    decltype(auto) value() const { return Operator::call(lhs); }
// NOLINTNEXTLINE
#define TEST_LHS_BINARY_OPERATOR(op, name)                     \
    template <class U>                                         \
    auto operator op(const U& rhs) const                       \
    {                                                          \
        return make_expression(lhs, rhs, name{}); /* NOLINT */ \
    }

    TEST_FOREACH_BINARY_OPERATORS(TEST_LHS_BINARY_OPERATOR)

// NOLINTNEXTLINE
#define TEST_LHS_UNARY_OPERATOR(op, name) \
    auto operator op() const { return make_lhs_expression(lhs, name{}); /* NOLINT */ }

    TEST_FOREACH_UNARY_OPERATORS(TEST_LHS_UNARY_OPERATOR)

// NOLINTNEXTLINE
#define TEST_LHS_REOPERATOR(op)                 \
    template <class U>                          \
    auto operator op(const U& rhs) const        \
    {                                           \
        return make_lhs_expression(lhs op rhs); \
    }
    TEST_LHS_REOPERATOR(+)
    TEST_LHS_REOPERATOR(-)
    TEST_LHS_REOPERATOR(*)
    TEST_LHS_REOPERATOR(/)
    TEST_LHS_REOPERATOR(%)
    TEST_LHS_REOPERATOR(&)
    TEST_LHS_REOPERATOR(|)
    TEST_LHS_REOPERATOR (^)
};

template <class F>
struct predicate
{
    std::string msg;
    F f;

    friend std::ostream& operator<<(std::ostream& s, const predicate& self)
    {
        s << self.msg;
        return s;
    }

    decltype(auto) operator()() const { return f(); }

    operator decltype(auto)() const { return f(); }
};

template <class F>
auto make_predicate(const std::string& msg, F f)
{
    return make_lhs_expression(predicate<F>{msg, f}, function{});
}

template <class T>
std::string as_string(const T& x)
{
    std::stringstream ss;
    ss << x;
    return ss.str();
}

template <class Iterator>
std::string as_string(Iterator start, Iterator last)
{
    std::stringstream ss;
    stream_range(ss, start, last);
    return ss.str();
}

template <class F>
auto make_function(const std::string& name, F f)
{
    return [=](auto&&... xs) {
        std::vector<std::string> args = {as_string(xs)...};
        return make_predicate(name + "(" + as_string(args.begin(), args.end()) + ")",
                              [=] { return f(xs...); });
    };
}

struct capture
{
    template <class T>
    auto operator->*(const T& x) const
    {
        return make_lhs_expression(x);
    }

    template <class T, class Operator>
    auto operator->*(const lhs_expression<T, Operator>& x) const
    {
        return x;
    }
};

enum class color
{
    reset      = 0,
    bold       = 1,
    underlined = 4,
    fg_red     = 31,
    fg_green   = 32,
    fg_yellow  = 33,
    fg_blue    = 34,
    fg_default = 39,
    bg_red     = 41,
    bg_green   = 42,
    bg_yellow  = 43,
    bg_blue    = 44,
    bg_default = 49
};
inline std::ostream& operator<<(std::ostream& os, const color& c)
{
#ifndef _WIN32
    static const bool use_color = isatty(STDOUT_FILENO) != 0;
    if(use_color)
        return os << "\033[" << static_cast<std::size_t>(c) << "m";
#endif
    return os;
}

template <class T, class F>
void failed(T x, const char* msg, const char* func, const char* file, int line, F f)
{
    if(!bool(x.value()))
    {
        std::cout << func << std::endl;
        std::cout << file << ":" << line << ":" << std::endl;
        std::cout << color::bold << color::fg_red << "    FAILED: " << color::reset << msg << " "
                  << "[ " << x << " ]" << std::endl;
        f();
    }
}

template <class F>
bool throws(F f)
{
    try
    {
        f();
        return false;
    }
    catch(...)
    {
        return true;
    }
}

template <class Exception, class F>
bool throws(F f, const std::string& msg = "")
{
    try
    {
        f();
        return false;
    }
    catch(const Exception& ex)
    {
        return std::string(ex.what()).find(msg) != std::string::npos;
    }
}

template <class T, class U>
auto near(T px, U py, double ptol = 1e-6f)
{
    return make_function("near", [](auto x, auto y, auto tol) { return std::abs(x - y) < tol; })(
        px, py, ptol);
}

using string_map = std::unordered_map<std::string, std::vector<std::string>>;

template <class Keyword>
string_map generic_parse(std::vector<std::string> as, Keyword keyword)
{
    string_map result;

    std::string flag;
    for(auto&& x : as)
    {
        auto f = keyword(x);
        if(f.empty())
        {
            result[flag].push_back(x);
        }
        else
        {
            flag = f.front();
            result[flag]; // Ensure the flag exists
            flag = f.back();
        }
    }
    return result;
}

using test_case = std::function<void()>;

inline auto& get_test_cases()
{
    // NOLINTNEXTLINE
    static std::vector<std::pair<std::string, test_case>> cases;
    return cases;
}

inline void add_test_case(std::string name, test_case f)
{
    get_test_cases().emplace_back(std::move(name), std::move(f));
}

struct auto_register_test_case
{
    template <class F>
    auto_register_test_case(const char* name, F f) noexcept
    {
        add_test_case(name, f);
    }
};

#if TEST_SANITIZE_ADDRESS
extern "C" {
// NOLINTNEXTLINE
void __sanitizer_start_switch_fiber(void** fake_stack_save, const void* bottom, size_t size);
// NOLINTNEXTLINE
void __sanitizer_finish_switch_fiber(void* fake_stack_save,
                                     const void** bottom_old,
                                     size_t* size_old);
}
struct asan_switch_stack
{
    void* s = nullptr;
    asan_switch_stack(char* stack, size_t size) { __sanitizer_start_switch_fiber(&s, stack, size); }
    ~asan_switch_stack()
    {
        const void* olds = nullptr;
        size_t size      = 0;
        __sanitizer_finish_switch_fiber(s, &olds, &size);
    }
};
#else
struct asan_switch_stack
{
    template <class... Ts>
    asan_switch_stack(Ts&&...)
    {
    }
};
#endif

[[noreturn]] inline void fail()
{
#ifdef __linux__
    std::abort();
#else
    throw std::runtime_error("FAILED");
#endif
}

#ifdef __linux__
extern "C" void __gcov_flush() __attribute__((weak)); // NOLINT
#endif

template <class F>
std::string fork(F f)
{
#ifdef __linux__
    static std::array<char, 8 * 1024 * 1024> stack = {};
    int pid                                        = clone(
        +[](void* g) -> int {
            asan_switch_stack s(stack.data(), stack.size());
            (*reinterpret_cast<F*>(g))();
            reinterpret_cast<F*>(g)->~F();
            if(__gcov_flush)
                __gcov_flush();
            std::quick_exit(0);
        },
        stack.data() + stack.size(),
        SIGCHLD | CLONE_PTRACE, // NOLINT
        &f);
    if(pid == -1)
        return "Unable to fork process";
    int status = -1;
    if(wait(&status) == -1)
        return "Wait error";
    if(WIFSIGNALED(status))                                                  // NOLINT
        return "Terminated with signal " + std::to_string(WTERMSIG(status)); // NOLINT
    if(not WIFEXITED(status))                                                // NOLINT
        return "Exited with " + std::to_string(WEXITSTATUS(status));         // NOLINT
    return {};
#else
    try
    {
        f();
    }
    catch(const std::exception& e)
    {
        return e.what();
    }
    catch(...)
    {
        return "Unknown exception";
    }
    return {};
#endif
}

struct driver
{
    driver()
    {
        add_flag({"--help", "-h"}, "Show help");
        add_flag({"--list", "-l"}, "List all test cases");
    }
    struct argument
    {
        std::vector<std::string> flags = {};
        std::string help               = "";
        int nargs                      = 1;
    };

    void add_arg(const std::vector<std::string>& flags, const std::string& help = "")
    {
        arguments.push_back(argument{flags, help, 1});
    }

    void add_flag(const std::vector<std::string>& flags, const std::string& help = "")
    {
        arguments.push_back(argument{flags, help, 0});
    }

    void show_help() const
    {
        std::cout << std::endl;
        std::cout << color::fg_yellow << "USAGE:" << color::reset << std::endl;
        std::cout << "    ";
        std::cout << "<exe> <options> <test-case>..." << std::endl;
        std::cout << std::endl;

        std::cout << color::fg_yellow << "ARGS:" << color::reset << std::endl;
        std::cout << "    ";
        std::cout << color::fg_green << "<test-case>..." << color::reset;
        std::cout << std::endl;
        std::cout << "        "
                  << "Test case name to run" << std::endl;
        std::cout << std::endl;
        std::cout << color::fg_yellow << "OPTIONS:" << color::reset << std::endl;
        for(auto&& arg : arguments)
        {
            std::string prefix = "    ";
            std::cout << color::fg_green;
            for(const std::string& a : arg.flags)
            {
                std::cout << prefix;
                std::cout << a;
                prefix = ", ";
            }
            std::cout << color::reset << std::endl;
            std::cout << "        " << arg.help << std::endl;
        }
    }

    string_map parse(int argc, const char* argv[]) const
    {
        std::vector<std::string> args(argv + 1, argv + argc);
        string_map keys;
        for(auto&& arg : arguments)
        {
            for(auto&& flag : arg.flags)
            {
                keys[flag] = {arg.flags.front()};
                if(arg.nargs == 0)
                    keys[flag].push_back("");
            }
        }
        return generic_parse(args, [&](auto&& s) -> std::vector<std::string> {
            if(keys.count(s) > 0)
                return keys[s];
            else
                return {};
        });
    }

    void run_test_case(const std::string& name, const test_case& f)
    {
        ran++;
        std::cout << color::fg_green << "[   RUN    ] " << color::reset << color::bold << name
                  << color::reset << std::endl;
        std::string msg = fork(f);
        if(msg.empty())
        {
            std::cout << color::fg_green << "[ COMPLETE ] " << color::reset << color::bold << name
                      << color::reset << std::endl;
        }
        else
        {
            failed.push_back(name);
            std::cout << color::fg_red << "[  FAILED  ] " << color::reset << color::bold << name
                      << color::reset << ": " << color::fg_yellow << msg << color::reset
                      << std::endl;
        }
    }

    void run(int argc, const char* argv[])
    {
        auto args = parse(argc, argv);
        if(args.count("--help") > 0)
        {
            show_help();
            return;
        }
        if(args.count("--list") > 0)
        {
            for(auto&& tc : get_test_cases())
                std::cout << tc.first << std::endl;
            return;
        }

        auto cases = args[""];
        if(cases.empty())
        {
            for(auto&& tc : get_test_cases())
                run_test_case(tc.first, tc.second);
        }
        else
        {
            std::unordered_map<std::string, test_case> m(get_test_cases().begin(),
                                                         get_test_cases().end());
            for(auto&& iname : cases)
            {
                for(auto&& name : get_case_names(iname))
                {
                    auto f = m.find(name);
                    if(f == m.end())
                    {
                        std::cout << color::fg_red << "[  ERROR   ] Test case '" << name
                                  << "' not found." << color::reset << std::endl;
                        failed.push_back(name);
                    }
                    else
                        run_test_case(name, f->second);
                }
            }
        }
        std::cout << color::fg_green << "[==========] " << color::fg_yellow << ran << " tests ran"
                  << color::reset << std::endl;
        if(not failed.empty())
        {
            std::cout << color::fg_red << "[  FAILED  ] " << color::fg_yellow << failed.size()
                      << " tests failed" << color::reset << std::endl;
            for(auto&& name : failed)
                std::cout << color::fg_red << "[  FAILED  ] " << color::fg_yellow << name
                          << color::reset << std::endl;
            std::exit(1);
        }
    }

    std::function<std::vector<std::string>(const std::string&)> get_case_names =
        [](const std::string& name) -> std::vector<std::string> { return {name}; };
    std::vector<argument> arguments = {};
    std::vector<std::string> failed = {};
    std::size_t ran                 = 0;
};

inline void run(int argc, const char* argv[])
{
    driver d{};
    d.run(argc, argv);
}

} // namespace test

// NOLINTNEXTLINE
#define CHECK(...)                                                                                 \
    test::failed(                                                                                  \
        test::capture{}->*__VA_ARGS__, #__VA_ARGS__, __PRETTY_FUNCTION__, __FILE__, __LINE__, [] { \
        })
// NOLINTNEXTLINE
#define EXPECT(...)                             \
    test::failed(test::capture{}->*__VA_ARGS__, \
                 #__VA_ARGS__,                  \
                 __PRETTY_FUNCTION__,           \
                 __FILE__,                      \
                 __LINE__,                      \
                 &test::fail)
// NOLINTNEXTLINE
#define STATUS(...) EXPECT((__VA_ARGS__) == 0)

// NOLINTNEXTLINE
#define TEST_CAT(x, ...) TEST_PRIMITIVE_CAT(x, __VA_ARGS__)
// NOLINTNEXTLINE
#define TEST_PRIMITIVE_CAT(x, ...) x##__VA_ARGS__

// NOLINTNEXTLINE
#define TEST_CASE_REGISTER(...)                                                    \
    static test::auto_register_test_case TEST_CAT(register_test_case_, __LINE__) = \
        test::auto_register_test_case(#__VA_ARGS__, &__VA_ARGS__);

// NOLINTNEXTLINE
#define TEST_CASE(...)              \
    void __VA_ARGS__();             \
    TEST_CASE_REGISTER(__VA_ARGS__) \
    void __VA_ARGS__()

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif

#endif
