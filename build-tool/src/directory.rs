use anyhow::{Result, bail};
use log::trace;
use std::env;
use std::path::{Path, PathBuf};

/// searches for build.cc starting from the given directory, optionally searching parent directories
pub fn find_build_path_from(start_dir: PathBuf, wants_backtrack_search: bool) -> Result<PathBuf> {
    let mut current_directory = start_dir;
    loop {
        let build_script = current_directory.join("build.cc");
        trace!("checking for build script at: {}", build_script.display());
        if build_script.exists() {
            trace!("found build script at: {}", build_script.display());
            return Ok(current_directory);
        }

        let parent_directory_exists = current_directory.parent().is_some();
        if !wants_backtrack_search || !parent_directory_exists {
            let message = if wants_backtrack_search {
                "no build script found in current or parent directories"
            } else {
                "no build script found in current directory"
            };
            bail!(message);
        }

        current_directory = current_directory.parent().unwrap().to_path_buf();
        trace!("moving up to parent directory: {}", current_directory.display());
    }
}

/// searches for build.cc starting from current directory
pub fn find_build_path(wants_backtrack_search: bool) -> Result<PathBuf> {
    find_build_path_from(env::current_dir()?, wants_backtrack_search)
}

/// resolves a path string to a PathBuf, handling tilde expansion and absolute/relative paths
fn resolve_path_string(path: String) -> Result<PathBuf> {
    let resolved_path = if path.starts_with("~/") {
        let home = dirs::home_dir().ok_or_else(|| anyhow::anyhow!("could not find home directory"))?;
        home.join(&path[2..])
    } else if Path::new(&path).is_absolute() {
        PathBuf::from(path)
    } else {
        env::current_dir()?.join(path)
    };

    // if the path points to build.cc, use its parent directory
    let resolved_path = if resolved_path.ends_with("build.cc") {
        if let Some(parent) = resolved_path.parent() {
            parent.to_path_buf()
        } else {
            bail!("build.cc file has no parent directory");
        }
    } else {
        resolved_path
    };

    if !resolved_path.exists() {
        bail!("path does not exist: {}", resolved_path.display());
    }

    if !resolved_path.is_dir() {
        bail!("path is not a directory: {}", resolved_path.display());
    }

    Ok(resolved_path)
}

pub fn resolve_working_directory(path: Option<String>, wants_backtrack_search: bool) -> Result<PathBuf> {
    match path {
        Some(path_str) => {
            let resolved_dir = resolve_path_string(path_str)?;
            if wants_backtrack_search {
                find_build_path_from(resolved_dir, true)
            } else {
                let build_script = resolved_dir.join("build.cc");
                if build_script.exists() {
                    Ok(resolved_dir)
                } else {
                    bail!("no build script found in specified directory: {}", resolved_dir.display());
                }
            }
        }
        None => find_build_path(wants_backtrack_search),
    }
}
