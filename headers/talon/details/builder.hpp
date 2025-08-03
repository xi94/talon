#pragma once

#include <sstream>
#include <string>
#include <string_view>

namespace talon {

struct build_script_builder {
    virtual ~build_script_builder() = default;

    [[nodiscard]] virtual auto get_script() const -> std::string = 0;

    virtual auto add_variable(std::string_view name, std::string_view value) -> void = 0;
    virtual auto add_rule(std::string_view name, std::string_view command, std::string_view description = "", std::string_view depfile = "",
                          std::string_view deps = "") -> void = 0;
    virtual auto add_build_edge(std::string_view output, std::string_view rule, std::string_view inputs) -> void = 0;

  protected:
    std::stringstream script_stream_;
};

struct ninja_builder final : build_script_builder {
    ninja_builder()
    {
        script_stream_ << "builddir = .talon/\n";
    }

    [[nodiscard]] auto get_script() const -> std::string override
    {
        return script_stream_.str();
    }

    auto add_variable(std::string_view name, std::string_view value) -> void override
    {
        script_stream_ << '\n' << name << " = " << value << '\n';
    }

    auto add_rule(std::string_view name, std::string_view command, std::string_view description, std::string_view depfile,
                  std::string_view deps) -> void override
    {
        script_stream_ << "\nrule " << name << "\n";
        script_stream_ << "  command = " << command << "\n";

        const bool has_deps = !deps.empty();
        if (has_deps) script_stream_ << "  deps = " << deps << '\n';

        const bool has_depfile = !depfile.empty();
        if (has_depfile) script_stream_ << "  depfile = " << depfile << '\n';

        const bool has_description = !description.empty();
        if (has_description) script_stream_ << "  description = " << description << '\n';
    }

    auto add_build_edge(std::string_view output, std::string_view rule, std::string_view inputs) -> void override
    {
        script_stream_ << "\nbuild " << output << ": " << rule << ' ' << inputs << '\n';
    }
};

} // namespace talon
