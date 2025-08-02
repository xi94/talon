#pragma once

#ifndef TALON_API
#define TALON_API
#endif

#include <cstdint>
#include <functional>
#include <string_view>
#include <algorithm>

namespace talon {

enum class platform : uint8_t {
    mac_os,
    linux_os,
    windows_os,
};

[[nodiscard]] constexpr auto to_string_view(const platform plat) noexcept -> std::string_view {
    switch (plat) {
    case platform::mac_os: return "MacOS";
    case platform::linux_os: return "Linux";
    case platform::windows_os: return "Windows";
    }

    // FIXME ugly fix but works for now
#ifdef _WIN32
    std::unreachable();
#else
    __builtin_unreachable();
#endif
}

#if defined(_WIN32)
constexpr platform os = platform::windows_os;
#elif defined(__APPLE__)
constexpr platform os = platform::mac_os;
#elif defined(__linux__)
constexpr platform os = platform::linux_os;
#else
#error yeah
#endif

enum class compilers : uint8_t {
    gcc,
    msvc,
    clang,
};

enum class cpp_versions : uint8_t {
    std_98 = 98,
    std_03 = 03,
    std_11 = 11,
    std_14 = 14,
    std_17 = 17,
    std_20 = 20,
    std_23 = 23,
};

enum class link_mode : uint8_t {
    statically,
    dynamically,
    mostly_static, // Links most dependencies statically, some dynamically
};

enum class sanitizer_mode : uint8_t {
    none,
    address,
    undefined,
    thread,
    memory,
    address_and_undefined,
};

enum class optimize_level : uint8_t {
    debug,     // -Og
    size,      // -Os
    speed,     // -O2
    max_speed, // -O3
};

enum class output_mode : uint8_t {
    executable,
    static_library,
    dynamic_library,
};

enum class build_systems : uint8_t {
    ninja,
};

// essentially vector<string_view> with custom methods
struct [[nodiscard]] arguments : std::vector<std::string_view> {
    using std::vector<std::string_view>::vector;

    [[nodiscard]] constexpr auto contains(const std::string_view arg) const noexcept -> bool {
        return std::ranges::contains(*this, arg);
    }
};

// a little awkward to have this struct in between the same namespace declared
// twice
struct build_options {
    compilers     compiler     = compilers::clang;
    cpp_versions  cpp_version  = cpp_versions::std_11;
    build_systems build_systen = build_systems::ninja;

    output_mode    output_type  = output_mode::executable;
    link_mode      link_mode    = link_mode::dynamically;
    sanitizer_mode sanitizer    = sanitizer_mode::none;
    optimize_level optimization = optimize_level::debug;

    bool print_build_script = false;

    // @Todo: maybe it would be good to have a check here,
    // to see what stage the token is used in, for example: "compile" or "build"
    // or even "compile and build"

    enum class compile_section : uint8_t {
        link,
        both,
        build,
    };

    struct compile_option {
        bool enabled = false;

        std::string_view msvc_flag;
        std::string_view clang_flag;

        compile_section section;

        constexpr auto operator=(const bool value) noexcept -> compile_option& {
            enabled = value;
            return *this;
        }

        [[nodiscard]] constexpr operator bool() const noexcept { return enabled; }
    };

    compile_option warn_all = {
        .msvc_flag  = "/Wall",
        .clang_flag = "-Wall",
        .section    = compile_section::build,
    };

    compile_option warn_extra = {
        .msvc_flag  = "/W4",
        .clang_flag = "-Wextra",
        .section    = compile_section::build,
    };

    compile_option warn_extra_tokens = {
        .msvc_flag  = {},
        .clang_flag = "-Wextra-tokens",
        .section    = compile_section::build,
    };

    compile_option warn_pedantic = {
        .msvc_flag  = "/permissive-",
        .clang_flag = "-Wpedantic",
        .section    = compile_section::build,
    };

    compile_option warn_old_style_casts = {
        .msvc_flag  = {},
        .clang_flag = "-Wold-style-cast",
        .section    = compile_section::build,
    };

    compile_option warn_cast_qualifiers = {
        .msvc_flag  = {},
        .clang_flag = "-Wcast-qual",
        .section    = compile_section::build,
    };

    compile_option warnings_are_errors = {
        .msvc_flag  = "/WX",
        .clang_flag = "-Werror",
        .section    = compile_section::build,
    };

    compile_option warn_unused = {
        .msvc_flag  = "/wd4101 /wd4102 /wd4189",
        .clang_flag = "-Wunused",
        .section    = compile_section::build,
    };

    compile_option warn_uninitialized = {
        .msvc_flag  = "/we4700",
        .clang_flag = "-Wuninitialized",
        .section    = compile_section::build,
    };

    compile_option warn_array_bounds = {
        .msvc_flag  = {},
        .clang_flag = "-Warray-bounds",
        .section    = compile_section::build,
    };

    compile_option warn_sign_conversion = {
        .msvc_flag  = "/we4365",
        .clang_flag = "-Wsign-conversion",
        .section    = compile_section::build,
    };

    compile_option warn_from_system_headers = {
        .msvc_flag  = "/external:W4",
        .clang_flag = "-Wsystem-headers",
        .section    = compile_section::build,
    };

    compile_option warn_shadow = {
        .msvc_flag  = "/we4456 /we4457 /we4458 /we4459",
        .clang_flag = "-Wshadow",
        .section    = compile_section::build,
    };

    compile_option warn_non_virtual_dtor = {
        .msvc_flag  = "/we4265",
        .clang_flag = "-Wnon-virtual-dtor",
        .section    = compile_section::build,
    };

    compile_option warn_conversion = {
        .msvc_flag  = "/we4244 /we4267",
        .clang_flag = "-Wconversion",
        .section    = compile_section::build,
    };

    compile_option warn_misleading_indentation = {
        .msvc_flag  = {},
        .clang_flag = "-Wmisleading-indentation",
        .section    = compile_section::build,
    };

    compile_option warn_null_dereference = {
        .msvc_flag  = {},
        .clang_flag = "-Wnull-dereference",
        .section    = compile_section::build,
    };

    compile_option warn_implicit_fallthrough = {
        .msvc_flag  = "/we5262",
        .clang_flag = "-Wimplicit-fallthrough",
        .section    = compile_section::build,
    };

    compile_option error_pedantic = {
        .msvc_flag  = "/permissive-",
        .clang_flag = "-pedantic-errors",
        .section    = compile_section::build,
    };

    compile_option strip_executable_symbols = {
        .msvc_flag  = "/DEBUG:NONE",
        .clang_flag = "-s",
        .section    = compile_section::build,
    };

    compile_option link_time_optimization = {
        .msvc_flag  = "/LTCG",
        .clang_flag = "-flto",
        .section    = compile_section::build,
    };

    compile_option debug_symbols = {
        .msvc_flag  = "/Zi",
        .clang_flag = "-g",
        .section    = compile_section::build,
    };

    compile_option warn_undef = {
        .msvc_flag  = {},
        .clang_flag = "-Wundef",
        .section    = compile_section::build,
    };

    compile_option warn_float_equal = {
        .msvc_flag  = {},
        .clang_flag = "-Wfloat-equal",
        .section    = compile_section::build,
    };

    compile_option warn_pointer_arith = {
        .msvc_flag  = {},
        .clang_flag = "-Wpointer-arith",
        .section    = compile_section::build,
    };

    compile_option warn_cast_align = {
        .msvc_flag  = {},
        .clang_flag = "-Wcast-align",
        .section    = compile_section::build,
    };

    compile_option warn_switch_default = {
        .msvc_flag  = "/w14062",
        .clang_flag = "-Wswitch-default",
        .section    = compile_section::build,
    };

    compile_option warn_switch_enum = {
        .msvc_flag  = "/w14061",
        .clang_flag = "-Wswitch-enum",
        .section    = compile_section::build,
    };

    compile_option warn_unreachable_code = {
        .msvc_flag  = "/w14702",
        .clang_flag = "-Wunreachable-code",
        .section    = compile_section::build,
    };

    compile_option warn_aggregate_return = {
        .msvc_flag  = {},
        .clang_flag = "-Waggregate-return",
        .section    = compile_section::build,
    };

    compile_option warn_write_strings = {
        .msvc_flag  = {},
        .clang_flag = "-Wwrite-strings",
        .section    = compile_section::build,
    };

    compile_option save_temps = {
        .msvc_flag  = "/EP",
        .clang_flag = "-save-temps",
        .section    = compile_section::build,
    };

    compile_option warn_strict_prototypes = {
        .msvc_flag  = {},
        .clang_flag = "-Wstrict-prototypes",
        .section    = compile_section::build,
    };

    compile_option warn_missing_prototypes = {
        .msvc_flag  = {},
        .clang_flag = "-Wmissing-prototypes",
        .section    = compile_section::build,
    };

    compile_option warn_old_style_definition = {
        .msvc_flag  = {},
        .clang_flag = "-Wold-style-definition",
        .section    = compile_section::build,
    };

    TALON_API auto visit_options(std::function<void(const compile_option&)>&& visitor) const -> void {
        visitor(warn_all);
        visitor(warn_extra);
        visitor(warn_extra_tokens);
        visitor(warn_pedantic);
        visitor(warn_old_style_casts);
        visitor(warn_cast_qualifiers);
        visitor(warnings_are_errors);
        visitor(warn_unused);
        visitor(warn_uninitialized);
        visitor(warn_array_bounds);
        visitor(warn_sign_conversion);
        visitor(warn_from_system_headers);
        visitor(warn_shadow);
        visitor(warn_non_virtual_dtor);
        visitor(warn_conversion);
        visitor(warn_misleading_indentation);
        visitor(warn_null_dereference);
        visitor(warn_implicit_fallthrough);
        visitor(error_pedantic);
        visitor(strip_executable_symbols);
        visitor(link_time_optimization);
        visitor(debug_symbols);
    }

    inline TALON_API auto enable_recommended_warnings() noexcept -> void {
        if (compiler == compilers::gcc || compiler == compilers::clang) {
            warn_all              = true;
            warn_extra            = true;
            warn_shadow           = true;
            warn_non_virtual_dtor = true;
            warn_pedantic         = true;
        }
    }
};

} // namespace talon
