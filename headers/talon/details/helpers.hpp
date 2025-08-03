#pragma once

#ifndef TALON_API
#define TALON_API
#endif

#include <algorithm>
#include <array>
#include <filesystem>
#include <optional>
#include <print>
#include <string_view>
#include <vector>

#include "build_options.hpp"

namespace talon {

namespace fs = std::filesystem;

namespace detail {

using compile_option = build_options::compile_option;
using compile_section = build_options::compile_section;

inline TALON_API auto parse_compile_flags(const build_options &opts) -> std::string
{
    std::string flag_buffer{};
    opts.visit_options([&](const compile_option &option) -> void {
        if (option.enabled && (option.section == compile_section::build || option.section == compile_section::both)) {
            flag_buffer += opts.compiler == compilers::msvc ? option.msvc_flag : option.clang_flag;
            flag_buffer += ' ';
        }
    });

    switch (opts.link_mode) {
    case link_mode::statically: {
        flag_buffer += "-static ";
        break;
    }

    case link_mode::dynamically: {
        break;
    }

    case link_mode::mostly_static: {
        flag_buffer += "-static-libgcc -static-libstdc++ ";
        break;
    }
    }

    switch (opts.sanitizer) {
    case sanitizer_mode::none: {
        break;
    }

    case sanitizer_mode::address: {
        flag_buffer += "-fsanitize=address ";
        break;
    }

    case sanitizer_mode::undefined: {
        flag_buffer += "-fsanitize=undefined ";
        break;
    }

    case sanitizer_mode::thread: {
        flag_buffer += "-fsanitize=thread ";
        break;
    }

    case sanitizer_mode::memory: {
        flag_buffer += "-fsanitize=memory ";
        break;
    }

    case sanitizer_mode::address_and_undefined: {
        flag_buffer += "-fsanitize=address,undefined ";
        break;
    }
    }

    switch (opts.optimization) {
    case optimize_level::debug: {
        flag_buffer += opts.compiler == compilers::msvc ? "/Od " : "-Og ";
        break;
    }

    case optimize_level::size: {
        flag_buffer += opts.compiler == compilers::msvc ? "/Os " : "-Os ";
        break;
    }

    case optimize_level::speed: {
        flag_buffer += opts.compiler == compilers::msvc ? "/O2 " : "-O2 ";
        break;
    }

    case optimize_level::max_speed: {
        flag_buffer += opts.compiler == compilers::msvc ? "/O2 " : "-O3 ";
        break;
    }
    }

    return flag_buffer;
}

inline TALON_API auto parse_link_flags(const build_options &opts) -> std::string
{
    std::string flag_buffer;
    opts.visit_options([&](const compile_option &option) -> void {
        if (option.enabled && (option.section == compile_section::link || option.section == compile_section::both)) {
            flag_buffer += opts.compiler == compilers::msvc ? option.msvc_flag : option.clang_flag;
            flag_buffer += ' ';
        }
    });

    return flag_buffer;
}

inline TALON_API constexpr auto compiler_to_statement(const compilers compiler) -> std::string_view
{
    switch (compiler) {
    case compilers::gcc: {
        return "g++";
    }

    case compilers::msvc: {
        return "cl";
    }

    case compilers::clang: {
        return "clang++";
    }
    }

    return "";
}

inline TALON_API auto find_and_collect_files(const fs::path &directory, const std::vector<std::string_view> &includes)
    -> std::vector<fs::path>
{
    const auto root = fs::path{directory / "src/"};
    if (!fs::exists(root) || !fs::is_directory(root)) { return {}; }

    static constexpr std::array target_extensions{".cc", ".cxx", ".cpp"};

    std::vector<fs::path> found_impl_files;
    found_impl_files.reserve(25000);

    auto iterate_and_append_implementation_files = [&](std::string_view target) -> std::optional<fs::filesystem_error> {
        try {
            for (const auto &entry : fs::recursive_directory_iterator{target}) {
                if (!entry.is_regular_file()) continue;

                const auto &path = entry.path();
                const auto extension = path.extension().string();
                if (std::ranges::contains(target_extensions, extension)) {
                    const auto file_name_with_relative_path = fs::relative(entry.path(), directory);
                    found_impl_files.push_back(std::move(file_name_with_relative_path));
                }
            }
        } catch (fs::filesystem_error ex) {
            return {ex};
        }

        return std::nullopt;
    };

    for (const auto &include : includes) {
        if (const auto ex = iterate_and_append_implementation_files(include)) {
            std::println(stderr, "[talon] error: path does not exist -> '{}'", ex->path1().string());
        }
    }

    return found_impl_files;
}

inline TALON_API auto cpp_version_to_statement(compilers compiler, cpp_versions cpp_version) -> std::string
{
    const auto version = static_cast<uint8_t>(cpp_version);
    if (compiler == compilers::msvc) {
        return version == 23 ? "/std:c++latest" : "/std:c++" + std::to_string(version);
    } else {
        return "-std=c++" + std::to_string(version);
    }
}

inline TALON_API auto format_include_directories(const std::vector<std::string_view> &includes, compilers compiler) -> std::string
{
    if (includes.empty()) return {};
    const auto flag = (compiler == compilers::msvc) ? "/I" : "-isystem";

    std::string result;
    for (const auto &include : includes) {
        result += flag + std::string{include} + ' ';
    }

    return result;
}

inline TALON_API auto format_force_includes(const std::vector<std::string_view> &force_includes, compilers compiler) -> std::string
{
    if (force_includes.empty()) return {};
    const auto flag = (compiler == compilers::msvc) ? "/FI" : "-include";

    std::string result;
    for (const auto &include : force_includes) {
        result += flag + std::string{include} + ' ';
    }

    return result;
}

inline TALON_API auto format_library_directories(const std::vector<std::string_view> &lib_paths, compilers compiler) -> std::string
{
    if (lib_paths.empty()) return {};
    const auto flag = (compiler == compilers::msvc) ? "/LIBPATH:" : "-L";

    std::string result;
    for (const auto &path : lib_paths) {
        result += flag + std::string{path} + ' ';
    }

    return result;
}

inline TALON_API auto format_library_files(const std::vector<std::string_view> &libraries, compilers compiler) -> std::string
{
    if (libraries.empty()) return {};

    std::string result;
    for (const auto &lib : libraries) {
        if (compiler == compilers::msvc) {
            result += std::string{lib} + ".lib ";
        } else {
            result += "-l" + std::string{lib} + ' ';
        }
    }

    return result;
}

inline TALON_API auto format_output_type_flags(output_mode output_type, compilers compiler) -> std::string
{
    if (compiler == compilers::msvc) {
        switch (output_type) {
        case output_mode::executable: return "";
        case output_mode::static_library: return " /c";
        case output_mode::dynamic_library: return " /LD";
        }
    } else { // clang
        switch (output_type) {
        case output_mode::executable: return "";
        case output_mode::static_library: return " -c";
        case output_mode::dynamic_library: return std::string{" -shared"} + (os == platform::linux_os ? " -fPIC" : "");
        }
    }

    return "";
}

inline TALON_API auto format_preprocessor_definitions(const std::vector<std::string_view> &definitions) -> std::string
{
    std::string result;
    for (const auto definition : definitions) {
        result += " -D";
        result += definition;
    }

    return result;
}

inline TALON_API auto format_output_link_flags(output_mode type, compilers compiler) -> std::string
{
    if (type == output_mode::dynamic_library && compiler == compilers::msvc) { return " /DLL"; }

    return "";
}

} // namespace detail

} // namespace talon
