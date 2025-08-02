#pragma once
#include <string_view>

namespace talon {

namespace blueprints {

constexpr std::string_view msvc = R"(
builddir = .talon/

rule compile
  command = {} /nologo /EHsc {} /Fo$out /Fd:build/vc140.pdb /c $in {} /FS /showIncludes /Zc:__cplusplus
  description = $in -> $out
  depfile = .talon/$out.d
  deps = msvc

rule link_exe
  command = {} /Fe$out $in {}
  description = [linked] -> $out

rule link_static
  command = lib /nologo /out:$out $in
  description = [archive] -> $out

rule link_shared
  command = {} /LD /Fe$out $in {}
  description = [shared] -> $out

{})";

constexpr std::string_view clang = R"(
builddir = .talon/

rule compile
  command = {} {} -o $out -c $in {} -MD -MF $out.d
  description = $in -> $out
  depfile = .talon/$out.d
  deps = gcc

rule link_exe
  command = {} -o $out $in {}
  description = [linked] -> $out

rule link_static
  command = ar rcs $out $in
  description = [archive] -> $out

rule link_shared
  command = {} -shared -o $out $in {}
  description = [shared] -> $out

{})";

}  // namespace blueprints

}  // namespace talon
