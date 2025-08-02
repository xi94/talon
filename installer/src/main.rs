use std::fs;
use std::{env, path::Path, process::Command};

fn remove_old_symlink(destination: &str) {
    match env::consts::OS {
        "windows" => {
            // for some reason fs::remove_file is unlink on linux, but remove_dir_all seems to be unlink on windows?
            if let Err(e) = fs::remove_dir_all(destination) {
                println!("Failed to remove symlink: {}", e);
            }
        }

        "linux" | "macos" => {
            let _ = Command::new("sudo").arg("unlink").arg(destination).status();
        }

        _ => panic!("unsupported platform: {}", env::consts::OS),
    }
}

// TODO add proper support for mac
fn create_symlink(source: &str, destination: &str) -> Command {
    // always unlink before linking, in order to avoid creating recursive symlinks
    remove_old_symlink(destination);

    #[cfg(unix)]
    {
        let mut cmd = Command::new("sudo");
        cmd.arg("ln").arg("-sf").arg(source).arg(destination);
        return cmd;
    }

    #[cfg(windows)]
    {
        let mut cmd = Command::new("cmd");
        cmd.args(&["/c", "mklink", "/D", destination, source]);
        return cmd;
    }
}

#[cfg(unix)]
fn create_symlink_to_headers(headers: &str) -> Command {
    create_symlink(headers, "/usr/include/talon")
}

#[cfg(windows)]
fn create_symlink_to_headers(headers: &str) -> Command {
    todo!()
}

#[cfg(unix)]
fn create_symlink_to_builder_executable(executable_path: &str) {
    create_symlink(executable_path, "/usr/local/bin/talon").status().unwrap();
}

#[cfg(windows)]
fn add_builder_executable_to_path(output_directory: &Path) {
    use winreg::RegKey;
    use winreg::enums::*;

    assert!(output_directory.exists(), "output directory does not exist");

    let hkcu = RegKey::predef(HKEY_CURRENT_USER);
    let environment = hkcu.open_subkey_with_flags("Environment", KEY_READ | KEY_WRITE).unwrap();

    let current_path: String = environment.get_value("PATH").unwrap_or_default();
    let new_directory = output_directory.display().to_string();

    if !current_path.contains(&new_directory) {
        let new_path = if current_path.is_empty() {
            new_directory.to_string()
        } else {
            format!("{};{}", current_path, new_directory)
        };

        println!("path result -> {:?}", &new_path);
        environment.set_value("PATH", &new_path).unwrap();
    }
}

fn is_elevated() -> bool {
    Command::new("net").args(&["session"]).output().map(|output| output.status.success()).unwrap_or(false)
}

fn request_elevation() {
    if !is_elevated() {
        println!("Requesting administrator privileges...");
        let exe_path = std::env::current_exe().unwrap();
        Command::new("powershell")
            .args(&["-Command", &format!("Start-Process '{}' -Verb RunAs", exe_path.display())])
            .spawn()
            .expect("Failed to restart with elevation");
        std::process::exit(0);
    }
}

#[cfg(windows)]
fn create_headers_symlinks_to_msvc_installations(talon_headers_directory: &str) {
    let mut found_include_directories = Vec::<String>::new();

    // TODO for any visual studio release <= 2019, the installation directory will be in the x86 directory
    let vs_root = Path::new("C:\\Program Files\\Microsoft Visual Studio\\2022");
    assert!(vs_root.exists(), "visual studio 2022 is not installed in the default directory?");

    println!("trying to find vs 2022 community installation");
    let vs_community = vs_root.join("Community");
    if vs_community.exists() {
        println!("found vs 2022 community installation");

        let vs_community_toolsets = vs_community.join("VC").join("Tools").join("MSVC");
        assert!(vs_community_toolsets.exists(), "vs 2022 community toolsets not found");

        if let Ok(toolsets) = fs::read_dir(&vs_community_toolsets) {
            for toolset in toolsets {
                if let Ok(toolset) = toolset {
                    let toolset_path = toolset.path();
                    println!("found toolset: {}", toolset.file_name().display());

                    let include_directory = toolset_path.join("include");
                    assert!(include_directory.exists(), "toolset has no include directory?");

                    // the 'talon' subdir might not exist, but it does not matter, as that is what the symlink demands
                    let target_directory = include_directory.join("talon");
                    found_include_directories.push(target_directory.display().to_string());
                }
            }
        }
    }

    for include_directory in found_include_directories {
        let status = create_symlink(talon_headers_directory, &include_directory).status().unwrap();
        if status.code().is_some() && status.code() != Some(0) {
            eprintln!("{:?}", status);
        }
    }
}

fn main() {
    let installer_directory = env!("CARGO_MANIFEST_DIR");
    let workspace_root = Path::new(installer_directory).parent().expect("unable to get installers parent");

    let headers_directory = workspace_root.join("headers").join("talon").display().to_string();
    let output_directory = workspace_root.join("bin").join("release");

    #[cfg(windows)]
    {
        request_elevation();
        add_builder_executable_to_path(&output_directory);
        create_headers_symlinks_to_msvc_installations(&headers_directory);
    }

    #[cfg(unix)]
    {
        let executable_path = executable_root_directory.join("talon").display().to_string();
        create_symlink_to_builder_executable(&executable_path);

        let mut cmd = create_symlink_to_headers(&headers_directory);
        cmd.status().expect("failed to create symlink to headers");
    }

    println!("\npress enter to exit...");
    std::io::stdin().read_line(&mut String::new()).unwrap();
}
