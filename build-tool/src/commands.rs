use crate::{cache, directory};
use anyhow::{Context, Result, bail};
use log::{debug, trace, warn};
use std::path::{Path, PathBuf};
use std::process::Command;
use std::{env, fs};

#[derive(Clone)]
pub struct OutputPath(pub String);
impl OutputPath {
    pub fn as_str(&self) -> &str {
        &self.0
    }
}

pub fn new(name: String) -> Result<()> {
    let cwd = env::current_dir()?;
    let target_directory = cwd.join(&name);
    if target_directory.exists() {
        bail!("path already exists '{}'", target_directory.display().to_string());
    }

    fs::create_dir_all(target_directory.join("src"))?;
    let main_content = r#"#include <iostream>

auto main() -> int {
    std::cout << "hello, talon!" << '\n';
    return 0;
}
"#;
    fs::write(target_directory.join("src/main.cpp"), main_content)?;
    let build_content = r#"#include <talon/talon.hpp>

auto build(talon::arguments args) -> void {
    auto workspace = talon::new_workspace();
    workspace->add_build_files("src/main.cpp");

    auto& opt = workspace->options;
    opt.compiler = talon::os == talon::windows_os ? talon::msvc : talon::clang;

    if (args.contains("-release")) {
	opt.optimization = talon::speed;
	std::printf("building release\n");
    }

    opt.warnings_are_errors = true;
    opt.enable_recommended_warnings();

    workspace->build();
}
"#;

    fs::write(target_directory.join("build.cc"), build_content)?;
    Ok(())
}

pub fn run(
    backtrack: bool,
    clean_first: bool,
    path: Option<String>,
    args: Vec<String>,
    forward: Vec<String>,
) -> Result<()> {
    let executable_path = build(backtrack, clean_first, path, args)?;
    trace!("running executable -> {:?}", &executable_path.0);

    _ = Command::new(executable_path.as_str()).args(forward).status()?;

    Ok(())
}

pub fn build(backtrack: bool, clean_first: bool, path: Option<String>, args: Vec<String>) -> Result<OutputPath> {
    // FIXME we are calling resolve_working_directory twice if we receive a clean commad
    if clean_first && let Err(err) = clean(backtrack, path.clone()) {
        warn!("failed to clean directory before build: {:?}", err);
    }

    let working_directory = directory::resolve_working_directory(path, backtrack)?;
    env::set_current_dir(&working_directory)
        .with_context(|| format!("failed to change to directory: {}", working_directory.display()))?;

    let build_script_path = Path::new("build.cc");
    let cache_directory = Path::new(".talon");
    let cache_hash_file = cache_directory.join("build_cache.txt");
    let cache_build_file: PathBuf = {
        let mut buffer = cache_directory.join("talon_build");
        if cfg!(windows) {
            buffer.set_extension("exe");
        }
        buffer
    };

    if !fs::exists(build_script_path)? {
        bail!("no build script found in path");
    }

    let project_output_executable_name =
        working_directory.file_name().context("working directory has no file name")?.to_string_lossy().to_string();

    // create cache directory if it doesnt exist
    fs::create_dir_all(cache_directory)
        .with_context(|| format!("failed to create cache directory: {}", cache_directory.display()))?;

    let current_hash = cache::hash_build_file(&build_script_path)
        .with_context(|| format!("failed to hash build script: {}", build_script_path.display()))?;

    if should_rebuild(&cache_hash_file, &current_hash)? {
        compile_builder(&build_script_path, &cache_build_file)?;
        update_cache(&cache_hash_file, &current_hash)?;
        println!("build script compilation finished");
    } else {
        println!("using cached builder (no changes detected)");
    }

    execute_builder(&cache_build_file, args)?;

    let output_path = Path::new("build").join(&project_output_executable_name);
    Ok(OutputPath(output_path.display().to_string()))
}

// FIXME not sure what to do, but now this folder runs full paths, as it has no use for relative
pub fn clean(backtrack: bool, path: Option<String>) -> Result<()> {
    trace!("clean command called");
    trace!("path: {:?}", path);
    trace!("backtrack: {}", backtrack);

    let working_directory = directory::resolve_working_directory(path, backtrack)?;
    if !fs::exists(working_directory.join("build.cc"))? {
        bail!("not inside of a talon project");
    }

    trace!("cleaning root: {}", working_directory.display());
    for dir in [".talon", "build"] {
        if fs::exists(working_directory.join(dir))? {
            trace!("removing {}", dir);
            fs::remove_dir_all(working_directory.join(dir))?;
        }
    }

    Ok(())
}

fn should_rebuild(cache_hash_file: &Path, current_hash: &str) -> Result<bool> {
    match std::fs::read_to_string(cache_hash_file) {
        Ok(cached_hash) => Ok(cached_hash.trim() != current_hash),
        Err(_) => {
            debug!("no cache file found, will rebuild");
            Ok(true)
        }
    }
}

fn compile_builder(build_script_path: &Path, output_path: &Path) -> Result<()> {
    println!("compiling builder");
    trace!("build script: {}", build_script_path.display());
    trace!("output path: {}", output_path.display());

    let output = if cfg!(windows) {
        compile_with_msvc(build_script_path, output_path)?
    } else {
        compile_with_clang(build_script_path, output_path)?
    };

    handle_compilation_output(output)?;
    Ok(())
}

fn compile_with_clang(build_script_path: &Path, output_path: &Path) -> Result<std::process::Output> {
    let output_path_str = output_path.display().to_string();
    let build_script_str = build_script_path.display().to_string();

    let mut arguments = vec!["-g", "-O0", "-o", &output_path_str, &build_script_str, "-std=c++23"];
    if cfg!(windows) {
        arguments.extend(["-target", "x86_64-pc-windows-msvc"]);
    }

    trace!("executing: clang++ {}", arguments.join(" "));
    Command::new("clang++").args(&arguments).output().context("Failed to execute clang++")
}

#[allow(unused)]
fn compile_with_msvc(build_script_path: &Path, output_path: &Path) -> Result<std::process::Output> {
    Command::new("cmd")
        .args([
            "/C",
            "cl",
            "/std:c++latest",
            "/EHsc",
            &build_script_path.display().to_string(),
            "/link",
            "/out:.talon/talon_build.exe",
        ])
        .output()
        .context("failed to execute Windows build command")
}

fn handle_compilation_output(output: std::process::Output) -> Result<()> {
    if !output.stdout.is_empty() {
        let stdout = String::from_utf8_lossy(&output.stdout);
        trace!("compiler stdout:\n{}", stdout);
    }

    if !output.stderr.is_empty() {
        let stderr = String::from_utf8_lossy(&output.stderr);
        trace!("compiler stderr:\n{}", stderr);
    }

    if !output.status.success() {
        let stderr = String::from_utf8_lossy(&output.stderr);
        let stdout = String::from_utf8_lossy(&output.stdout);

        // FIXME this is awful, the newlines and what not all go through to the output
        eprintln!("{:?}", stderr.to_string());
        eprintln!("{:?}", stdout.to_string());

        bail!("compilation failed");
    }

    Ok(())
}

fn update_cache(cache_hash_file: &Path, current_hash: &str) -> Result<()> {
    cache::update_build_cache(cache_hash_file, current_hash)
        .with_context(|| format!("failed to update cache file: {}", cache_hash_file.display()))
}

fn execute_builder(cache_build_file: &Path, args: Vec<String>) -> Result<()> {
    debug!("executing builder: {}", cache_build_file.display());

    let mut cmd = Command::new(cache_build_file);
    for arg in args.iter() {
        cmd.arg(arg);
    }

    let status = cmd.status().with_context(|| format!("failed to execute builder: {}", cache_build_file.display()))?;
    if status.code() != Some(0) || !status.success() {
        return Err(anyhow::anyhow!("builder failed to compile"));
    }

    Ok(())
}
