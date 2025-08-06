#include <algorithm>
#include <array>
#include <cstddef>
#include <cxxabi.h>
#include <string_view>
#include <typeinfo>

// modified from https://stackoverflow.com/questions/1055452/c-get-name-of-type-in-template/59522794#59522794
namespace {
template <typename T>
[[nodiscard]] constexpr std::string_view function_signature() {
#ifndef _MSC_VER
    return __PRETTY_FUNCTION__;
#else
    return __FUNCSIG__;
#endif
}

struct TypeNameFormat {
    std::size_t junk_leading = 0;
    std::size_t junk_total = 0;
};

constexpr TypeNameFormat type_name_format = [] {
    TypeNameFormat ret;
    std::string_view sample = function_signature<int>();
    ret.junk_leading = sample.find("int");
    ret.junk_total = sample.size() - 3;
    return ret;
}();
static_assert(type_name_format.junk_leading != std::size_t(-1), "Unable to determine the type name format on this compiler.");

template <typename T>
static constexpr auto type_name_storage = [] {
    std::array<char, function_signature<T>().size() - type_name_format.junk_total + 1> ret{};
    std::copy_n(function_signature<T>().data() + type_name_format.junk_leading, ret.size() - 1, ret.data());
    return ret;
}();
}

template <typename T>
[[nodiscard]] constexpr std::string_view type_name() {
    return {type_name_storage<T>.data(), type_name_storage<T>.size() - 1};
}

template <typename T>
[[nodiscard]] constexpr const char *type_name_c() {
    return type_name_storage<T>.data();
}

template <class T>
std::string type_name(const T &obj) {
    int status;
    std::unique_ptr<char, void (*)(void *)> res{
        abi::__cxa_demangle(typeid(obj).name(), nullptr, nullptr, &status),
        std::free};
    return (status == 0) ? res.get() : typeid(obj).name();
}

// Generic helper to determine the "more exact" type, preferring floating point types
template <typename T1, typename T2>
struct MoreExactType {
private:
    static constexpr bool t1_is_fp = std::is_floating_point_v<T1>;
    static constexpr bool t2_is_fp = std::is_floating_point_v<T2>;

public:
    using type = std::conditional_t<
        (t1_is_fp && !t2_is_fp), T1,
        std::conditional_t<(t2_is_fp && !t1_is_fp), T2,
                           std::conditional_t<(sizeof(T1) >= sizeof(T2)), T1, T2>>>;
};

template <typename T1, typename T2>
using MoreExact = typename MoreExactType<T1, T2>::type;