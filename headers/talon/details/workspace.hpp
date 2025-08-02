#pragma once

#ifndef TALON_API
#define TALON_API
#endif

#include <format>
#include <fstream>
#include <string>

#include "blueprints.hpp"
#include "build_options.hpp"
#include "helpers.hpp"

namespace talon {

namespace detail {

template <typename T>
concept string_view_implicit =
    std::same_as<std::decay_t<T>, const char*> || std::same_as<std::decay_t<T>, std::string_view>;

} // namespace detail

struct workspace {
    build_options options     = {};
    fs::path      root        = fs::current_path();
    std::string   output_name = root.filename().string();

    std::vector<std::string_view> build_files;
    std::vector<std::string_view> build_file_search_paths;
    std::vector<std::string_view> include_directories;
    std::vector<std::string_view> force_include_files;
    std::vector<std::string_view> preprocessor_definitions;
    std::vector<std::string_view> library_include_directories;
    std::vector<std::string_view> library_files;
    std::vector<std::string_view> additional_linker_flags;

    template <detail::string_view_implicit... Args>
    inline TALON_API auto add_build_files(Args&&... files) noexcept -> void {
        (build_files.push_back(std::forward<Args&&>(files)), ...);
    }

    template <detail::string_view_implicit... Args>
    inline TALON_API auto add_all_build_files(Args&&... files) noexcept -> void {
        (build_file_search_paths.push_back(files), ...);
    }

    template <detail::string_view_implicit... Args>
    inline TALON_API auto add_includes(Args&&... include_folders) noexcept -> void {
        (include_directories.push_back(std::forward<Args&&>(include_folders)), ...);
    }

    template <detail::string_view_implicit... Args>
    inline TALON_API auto add_force_includes(Args&&... include_files) noexcept -> void {
        (force_include_files.push_back(std::forward<Args&&>(include_files)), ...);
    }

    template <detail::string_view_implicit... Args>
    inline TALON_API auto add_definitions(Args&&... definitions) noexcept -> void {
        (preprocessor_definitions.push_back(std::forward<Args&&>(definitions)), ...);
    }

    template <detail::string_view_implicit... Args>
    inline TALON_API auto add_library_includes(Args&&... include_folders) noexcept -> void {
        (library_include_directories.push_back(std::forward<Args&&>(include_folders)), ...);
    }

    template <detail::string_view_implicit... Args>
    inline TALON_API auto add_library_files(Args&&... include_files) noexcept -> void {
        (library_files.push_back(std::forward<Args&&>(include_files)), ...);
    }

    inline TALON_API auto set_build_options(const build_options new_options) -> void {
        options = std::move(new_options);
    }

    inline TALON_API auto build() noexcept -> void {
        const bool using_msvc = options.compiler == compilers::msvc;
        const bool on_windows = os == platform::windows_os;
        if (using_msvc && !on_windows) {
            fprintf(stderr, "[talon] error: compiler set to MSVC, but platform is not Windows\n");
            std::exit(1);
        }

        const bool wants_debug_symbols         = options.debug_symbols.enabled;
        const bool wants_release_optimizations = options.optimization > optimize_level::debug;
        if (wants_debug_symbols && wants_release_optimizations) {
            options.optimization = optimize_level::debug;
            fprintf(stderr, "[talon] warning: generate debug symbols is on, optimization set to debug\n");
        }

        add_file_extension(output_name, options.output_type);
        const auto build_script = create_build_script();
        if (options.print_build_script) {
            printf("----------------------------------------------------------------\n");
            printf("build file\n");
            printf("----------------------------------------------------------------\n");
            printf("%s\n", build_script.data());
            printf("----------------------------------------------------------------\n");
        }

        const auto cache_directory         = root / ".talon/";
        const bool missing_cache_directory = !fs::exists(cache_directory);
        if (missing_cache_directory && !fs::create_directory(cache_directory)) {
            fprintf(stderr, "[talon] error: failed to create cache directory\n");
            std::exit(1);
        }

        auto ninja_build_script_file = std::ofstream{cache_directory / "build.ninja"};
        ninja_build_script_file << build_script;
        ninja_build_script_file.close();

        const bool builder_executed_successfully = std::system("ninja -f .talon/build.ninja") == 0;
        if (!builder_executed_successfully) std::exit(1);

        printf("\n");
    }

  private:
    static TALON_API auto add_file_extension(std::string& output_name, const output_mode type) -> void {
        switch (os) {
        case platform::mac_os: {
            switch (type) {
            case output_mode::executable: break;
            case output_mode::static_library: output_name += ".a"; break;
            case output_mode::dynamic_library: output_name += ".dylib"; break;
            }

            break;
        }

        case platform::linux_os: {
            switch (type) {
            case output_mode::executable: break;
            case output_mode::static_library: output_name += ".a"; break;
            case output_mode::dynamic_library: output_name += ".so"; break;
            }

            break;
        }

        case platform::windows_os: {
            switch (type) {
            case output_mode::executable: output_name += ".exe"; break;
            case output_mode::static_library: output_name += ".lib"; break;
            case output_mode::dynamic_library: output_name += ".dll"; break;
            }

            break;
        }
        }
    }

    inline TALON_API auto create_build_script() const -> std::string {
        const auto compile_flags = [&] -> std::string {
            const auto directories   = detail::format_include_directories(include_directories, options.compiler);
            const auto include_files = detail::format_force_includes(force_include_files, options.compiler);
            const auto definitions   = detail::format_preprocessor_definitions(preprocessor_definitions);
            const auto output_flags  = detail::format_output_type_flags(options.output_type, options.compiler);

            return detail::parse_compile_flags(options) + directories + include_files + definitions + output_flags;
        }();

        const auto link_flags = [&] -> std::string {
            std::string all_linker_options;

            all_linker_options += detail::parse_link_flags(options);
            all_linker_options += detail::format_library_files(library_files, options.compiler);
            all_linker_options += detail::format_library_directories(library_include_directories, options.compiler);
            all_linker_options += detail::format_output_link_flags(options.output_type, options.compiler);

            for (const auto& flag : additional_linker_flags) {
                all_linker_options += std::string{flag} + ' ';
            }

            // for msvc, prepend the entire set of options with /link just once
            if (options.compiler == compilers::msvc && !all_linker_options.empty()) {
                return "/link " + all_linker_options;
            }
            return all_linker_options;
        }();

        const auto version    = detail::cpp_version_to_statement(options.compiler, options.cpp_version);
        const auto compiler   = detail::compiler_to_statement(options.compiler);
        const auto statements = create_build_statements();

        if (options.compiler == compilers::msvc) {
            return std::format(blueprints::msvc, compiler, version, compile_flags, compiler, link_flags, compiler,
                               link_flags, statements);
        } else {
            return std::format(blueprints::clang, compiler, version, compile_flags, compiler, link_flags, compiler,
                               link_flags, statements);
        }
    }

    inline TALON_API auto create_build_statements() const -> std::string {
        const auto  object_extension = options.compiler == compilers::msvc ? ".obj" : ".o";
        std::string link_inputs;
        std::string build_statements;
        const auto  found_files           = detail::find_and_collect_files(root, build_file_search_paths);
        const auto  number_of_build_files = build_files.size() + found_files.size();

        std::vector<std::string> all_build_files;
        all_build_files.reserve(number_of_build_files);
        for (const auto path : build_files) {
            all_build_files.push_back(path.data());
        }

        for (const auto& path : found_files) {
            all_build_files.push_back(path.string());
        }

        for (const auto& file : all_build_files) {
            const auto object = fs::path{file}.replace_extension(object_extension).string();
            link_inputs += std::format(" build/objects/{}", object);
            build_statements += std::format("build build/objects/{}: compile {}\n", object, file);
        }

        const auto link_rule = [type = options.output_type] -> std::string {
            switch (type) {
            case output_mode::executable: return "link_exe";
            case output_mode::static_library: return "link_static";
            case output_mode::dynamic_library: return "link_shared";
            default: return "link_exe"; // @Fallback
            }
        }();

        build_statements += std::format("build build/{}: {}{}\n", output_name, link_rule, link_inputs);
        return build_statements;
    }
};
} // namespace talon
