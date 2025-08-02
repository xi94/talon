use log::trace;
use sha2::{Digest, Sha256};
use std::fs;
use std::path::Path;

use anyhow::Result;

pub fn hash_build_file(build_script: &Path) -> Result<String> {
    trace!("computing hash for: {}", build_script.display());
    let hash = hex::encode(Sha256::digest(fs::read(build_script)?));

    trace!("build script hash: {}", hash);
    Ok(hash)
}

pub fn update_build_cache(cache_file_path: &Path, hash: &str) -> Result<()> {
    trace!("updating cache file: {}", cache_file_path.display());
    fs::write(cache_file_path, hash)?;
    Ok(())
}
