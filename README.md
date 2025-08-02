# talon
a very incomplete build system for c++

## build instructions
unix: ```cargo build --release && bin/release/installer```\
windows: ```cargo build --release && bin\release\installer.exe```

## command line options
<img width="772" height="419" alt="{65212FFA-B5E4-48CA-85B2-EA8D0F39DE0D}" src="https://github.com/user-attachments/assets/e636064d-6e58-4403-9a92-eb35a1ea5ad9" />

## example build script
```c++
#include <talon/talon.hpp>

auto set_build_options(const talon::arguments &args, talon::build_options *opts) -> void {
    opts->compiler = talon::clang;
    opts->cpp_version = talon::std_23;

    opts->warnings_are_errors = true;
    opts->enable_recommended_warnings(); // recommended warnings according to cppbestpractices

    if (args.contains("release")) {
        std::printf("building release\n");
        opts->optimization = talon::release;
    }
}

auto set_build_dependencies(talon::workspace *w) -> void {
    w->add_includes("src");
    w->add_build_files("src/main.cc"); // alternatively, you can add every impl file in a specified folder: w->add_all_build_files("src");
}

auto build(talon::arguments args) -> void {
    auto workspace = talon::workspace{};

    set_build_dependencies(&workspace);
    set_build_options(args, &workspace.options);

    workspace.build();
}
```
