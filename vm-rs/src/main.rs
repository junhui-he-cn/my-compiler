mod bytecode;
mod format;
mod runtime;
mod value;
mod vm;

use std::env;
use std::fs;
use std::process;

const HELP: &str = "compiler-design-vm 0.1.0\n\n\
Usage:\n\
  compiler-design-vm --help\n\
  compiler-design-vm dump <program.cdbc>\n\
  compiler-design-vm run <program.cdbc>\n\n\
Current phase: .cdbc parsing, canonical dump, and bytecode execution are implemented.\n";

fn help_text() -> &'static str {
    HELP
}

fn dump(path: &str) -> Result<(), String> {
    let source = fs::read_to_string(path)
        .map_err(|error| format!("error: failed to read `{}`: {}", path, error))?;
    let program = format::parse_program(&source).map_err(|error| format!("error: {}", error))?;
    print!("{}", format::format_program(&program));
    Ok(())
}

fn run(path: &str) -> Result<(), String> {
    let source = fs::read_to_string(path)
        .map_err(|error| format!("error: failed to read `{}`: {}", path, error))?;
    let program = format::parse_program(&source).map_err(|error| format!("error: {}", error))?;
    let output = vm::VM::new(&program)
        .run()
        .map_err(|error| error.to_string())?;
    print!("{}", output);
    Ok(())
}

fn main() {
    let mut args = env::args().skip(1);
    match args.next().as_deref() {
        None | Some("-h") | Some("--help") => {
            print!("{}", help_text());
        }
        Some("dump") => {
            let Some(path) = args.next() else {
                eprintln!("error: dump expects <program.cdbc>");
                eprintln!();
                eprint!("{}", help_text());
                process::exit(64);
            };
            if args.next().is_some() {
                eprintln!("error: dump expects exactly one input file");
                eprintln!();
                eprint!("{}", help_text());
                process::exit(64);
            }
            if let Err(error) = dump(&path) {
                eprintln!("{}", error);
                process::exit(1);
            }
        }
        Some("run") => {
            let Some(path) = args.next() else {
                eprintln!("error: run expects <program.cdbc>");
                eprintln!();
                eprint!("{}", help_text());
                process::exit(64);
            };
            if args.next().is_some() {
                eprintln!("error: run expects exactly one input file");
                eprintln!();
                eprint!("{}", help_text());
                process::exit(64);
            }
            if let Err(error) = run(&path) {
                eprintln!("{}", error);
                process::exit(1);
            }
        }
        Some(command) => {
            eprintln!("error: unknown command `{}`", command);
            eprintln!();
            eprint!("{}", help_text());
            process::exit(64);
        }
    }
}

#[cfg(test)]
mod tests {
    use super::help_text;

    #[test]
    fn help_mentions_dump_and_run_scope() {
        let help = help_text();
        assert!(help.contains("compiler-design-vm dump <program.cdbc>"));
        assert!(help.contains("compiler-design-vm run <program.cdbc>"));
        assert!(
            help.contains(".cdbc parsing, canonical dump, and bytecode execution are implemented")
        );
    }
}
