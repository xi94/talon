mod cache;
mod commands;
mod directory;

use anyhow::Result;
use clap::{Parser, Subcommand};

#[derive(Parser)]
#[command(name = "talon")]
#[command(version = env!("CARGO_PKG_VERSION"))]
#[command(about = concat!("talon ", env!("CARGO_PKG_VERSION"), " - modern build system for c++ projects"))]
#[command(after_help = "for command-specific help, see: talon help <command>")]
struct Cli {
    #[command(subcommand)]
    command: Commands,

    #[arg(short, long, help = "Enable verbose output")]
    verbose: bool,

    #[arg(short, long, help = "Suppress all output except errors")]
    quiet: bool,
}

#[derive(Debug, Subcommand)]
enum Commands {
    /// Creates a new base template project
    New { name: String },

    /// Builds and runs the assumed (or specified) project
    Run {
        /// Searches backwards for a talon build script
        #[arg(short, long)]
        backtrack: bool,

        /// Clean builds the project
        #[arg(short, long)]
        clean: bool,

        /// Path to the talon project
        path: Option<String>,

        /// Gets sent as an argument to builder, used to set the build profile
        #[arg(short, long = "profile")]
        profile_args: Vec<String>,

        /// Gets sent to the output executable
        #[arg(trailing_var_arg = true, last = true)]
        output_args: Vec<String>,
    },

    /// Builds the assumed (or specified) project
    Build {
        /// Searches backwards for a talon build script
        #[arg(short, long)]
        backtrack: bool,

        /// Clean builds the project
        #[arg(short, long)]
        clean: bool,

        /// Path to the talon project
        path: Option<String>,

        /// Gets sent as an argument to builder, used to set the build profile
        #[arg(short, long = "profile")]
        profile_args: Vec<String>,
    },

    /// Cleans the assumed (or specified) project by removing the build and cache
    Clean {
        /// Searches backwards for a talon build script
        #[arg(short, long)]
        backtrack: bool,

        /// Searches backwards for a talon build script
        path: Option<String>,
    },
}

fn setup_logging(verbose: bool, quiet: bool) {
    let level = if quiet {
        log::LevelFilter::Error
    } else if verbose {
        log::LevelFilter::Trace
    } else {
        log::LevelFilter::Info
    };

    env_logger::Builder::from_default_env()
        .filter_level(level)
        .format_timestamp(None)
        .format_module_path(false)
        .format_target(false)
        .init();
}

fn main() -> Result<()> {
    let cli = Cli::parse();
    setup_logging(cli.verbose, cli.quiet);

    match cli.command {
        Commands::New { name } => commands::new(name)?,
        Commands::Clean { backtrack, path } => commands::clean(backtrack, path)?,

        Commands::Build { backtrack, clean, path, profile_args } => {
            _ = commands::build(backtrack, clean, path, profile_args)?
        }

        Commands::Run { backtrack, clean, path, profile_args, output_args } => {
            commands::run(backtrack, clean, path, profile_args, output_args)?
        }
    }

    Ok(())
}
