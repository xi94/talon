#pragma once

#ifndef TALON_API
#define TALON_API
#endif

#include "details/build_options.hpp"
#include "details/workspace.hpp"

namespace talon
{

using enum platform;
using enum compilers;
using enum output_mode;
using enum cpp_versions;
using enum optimize_level;

} // namespace talon

auto build(talon::arguments) -> void;

auto main(int argc, char* argv[]) -> int
{
    char** begin = argv + 1; // skip first argument
    char** end = argv + argc;

    const auto args = talon::arguments(begin, end);
    build(std::move(args));

    return 0;
}
