#pragma once

#ifndef TALON_API
#define TALON_API
#endif

#include <format>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "build_options.hpp"
#include "builder.hpp"
#include "helpers.hpp"

namespace talon {

namespace detail {

template <typename T>
concept string_view_implicit = std::same_as<std::decay_t<T>, const char *> || std::same_as<std::decay_t<T>, std::string_view>;

} // namespace detail

struct workspace {
    build_options options = {};
    fs::path root = fs::current_path();
    std::string output_name = root.filename().string();

    std::vector<std::string_view> build_files;
    std::vector<std::string_view> build_file_search_paths;
    std::vector<std::string_view> include_directories;
    std::vector<std::string_view> preprocessor_definitions;
    std::vector<std::string_view> library_include_directories;
    std::vector<std::string_view> library_files;
    std::vector<std::string_view> additional_linker_flags;

    std::string_view windows_resource_file;

    template <detail::string_view_implicit... Args>
    inline TALON_API auto add_build_files(Args &&...files) noexcept -> void
    {
        (build_files.push_back(std::forward<Args>(files)), ...);
    }

    template <detail::string_view_implicit... Args>
    inline TALON_API auto add_source_directories(Args &&...paths) noexcept -> void
    {
        (build_file_search_paths.push_back(std::forward<Args>(paths)), ...);
    }

    template <detail::string_view_implicit... Args>
    inline TALON_API auto add_includes(Args &&...folders) noexcept -> void
    {
        (include_directories.push_back(std::forward<Args>(folders)), ...);
    }

    template <detail::string_view_implicit... Args>
    inline TALON_API auto add_definitions(Args &&...defs) noexcept -> void
    {
        (preprocessor_definitions.push_back(std::forward<Args>(defs)), ...);
    }

    template <detail::string_view_implicit... Args>
    inline TALON_API auto add_library_includes(Args &&...folders) noexcept -> void
    {
        (library_include_directories.push_back(std::forward<Args>(folders)), ...);
    }

    template <detail::string_view_implicit... Args>
    inline TALON_API auto add_libraries(Args &&...libs) noexcept -> void
    {
        (library_files.push_back(std::forward<Args>(libs)), ...);
    }

    template <detail::string_view_implicit... Args>
    inline TALON_API auto add_linker_flags(Args &&...flags) noexcept -> void
    {
        (additional_linker_flags.push_back(std::forward<Args>(flags)), ...);
    }

    constexpr auto set_windows_resource_file(std::string_view path) noexcept -> void
    {
        if constexpr (os == platform::windows_os) windows_resource_file = path;
    }

    inline TALON_API auto set_build_options(const build_options &new_options) -> void
    {
        options = new_options;
    }

    inline TALON_API auto build() noexcept -> void
    {
        if (options.compiler == compilers::msvc && os != platform::windows_os) {
            fprintf(stderr, "[talon] error: MSVC compiler is only supported on Windows\n");
            std::exit(1);
        }

        if (options.debug_symbols && options.optimization > optimize_level::debug) {
            options.optimization = optimize_level::debug;
            fprintf(stderr, "[talon] warning: debug symbols enabled, forcing optimization to debug level\n");
        }

        add_file_extension(output_name, options.output_type);
        const auto build_script = create_build_script();

        if (options.print_build_script) printf("--- build.ninja ---\n%s\n-------------------\n", build_script.data());

        const auto cache_directory = root / ".talon/";
        if (!fs::exists(cache_directory)) { fs::create_directory(cache_directory); }

        const auto build_directory = root / "build/";
        if (!fs::exists(build_directory / "objects")) { fs::create_directories(build_directory / "objects"); }

        std::ofstream{cache_directory / "build.ninja"} << build_script;
        if (std::system("ninja -f .talon/build.ninja") != 0) {
            fprintf(stderr, "[talon] error: build failed.\n");
            std::exit(1);
        }

        printf("[talon] build successful: %s\n", (build_directory / output_name).string().c_str());
    }

  private:
    auto create_builder() const -> std::unique_ptr<build_script_builder>
    {
        return std::make_unique<ninja_builder>();
    }

    static TALON_API auto add_file_extension(std::string &name, const output_mode type) -> void
    {
        if (os == platform::windows_os) {
            switch (type) {
            case output_mode::executable: name += ".exe"; break;
            case output_mode::static_library: name += ".lib"; break;
            case output_mode::dynamic_library: name += ".dll"; break;
            }
        }
    }

    TALON_API auto create_build_script() const -> std::string
    {
        auto builder = create_builder();

        const auto compiler_path = detail::compiler_to_statement(options.compiler);
        builder->add_variable("cxx", compiler_path);

        std::string cflags;
        cflags += detail::parse_compile_flags(options);
        cflags += detail::cpp_version_to_statement(options.compiler, options.cpp_version) + " ";
        cflags += detail::format_include_directories(include_directories, options.compiler);
        cflags += detail::format_preprocessor_definitions(preprocessor_definitions);
        builder->add_variable("cflags", cflags);

        std::string lflags;
        lflags += detail::parse_link_flags(options);
        lflags += detail::format_library_directories(library_include_directories, options.compiler);
        lflags += detail::format_library_files(library_files, options.compiler);
        for (const auto &flag : additional_linker_flags) {
            lflags += std::string{flag} + ' ';
        }

        if (options.compiler == compilers::msvc && !lflags.empty()) lflags = "/link " + lflags;
        builder->add_variable("lflags", lflags);

        // TODO support for other platforms
        std::string_view link_rule_name;
        const bool has_icon = !windows_resource_file.empty();

        if (options.compiler == compilers::msvc) {
            builder->add_rule("compile", "$cxx /nologo /EHsc /Fo$out /Fd:build/vc140.pdb /c $in $cflags /FS /showIncludes /Zc:__cplusplus",
                              "Compiling $in", ".talon/$out.d", "msvc");

            if (has_icon) builder->add_rule("compile_rc", "rc.exe /nologo /fo$out $in", "Compiling resource $in");

            switch (options.output_type) {
            case output_mode::executable: {
                link_rule_name = "link_exe";
                builder->add_rule(link_rule_name, "$cxx /Fe$out $in $lflags", "Linking executable $out");
                break;
            }

            case output_mode::static_library: {
                link_rule_name = "link_static_lib";
                builder->add_rule(link_rule_name, "lib /nologo /out:$out $in", "Archiving static library $out");
                break;
            }

            case output_mode::dynamic_library: {
                link_rule_name = "link_shared_lib";
                builder->add_rule(link_rule_name, "$cxx /LD /Fe$out $in $lflags", "Linking shared library $out");
                break;
            }
            }
        }

        std::stringstream link_inputs_stream;
        const auto object_extension = (options.compiler == compilers::msvc) ? ".obj" : ".o";

        auto all_source_files = detail::find_and_collect_files(root, build_file_search_paths);
        for (const auto &file_sv : build_files) {
            all_source_files.push_back(fs::path{file_sv});
        }

        for (const auto &file : all_source_files) {
            auto object_path = file;
            object_path.replace_extension(object_extension);
            const auto object_output = "build/objects/" + object_path.string();

            builder->add_build_edge(object_output, "compile", file.string());
            link_inputs_stream << " " << object_output;
        }

        // TODO icon support for other platforms
        if (os == platform::windows_os && has_icon) {
            const auto res_output = "build/" + fs::path{windows_resource_file}.stem().string() + ".res";
            builder->add_build_edge(res_output, "compile_rc", windows_resource_file);
            link_inputs_stream << " " << res_output;
        }

        const auto final_output = "build/" + output_name;
        builder->add_build_edge(final_output, link_rule_name, link_inputs_stream.str());

        return builder->get_script();
    }
};

} // namespace talon
