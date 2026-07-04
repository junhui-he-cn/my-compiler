mod bytecode;
mod format;
mod runtime;
mod value;

use std::env;
use std::fs;
use std::process;

const HELP: &str = "compiler-design-vm 0.1.0\n\n\
Usage:\n\
  compiler-design-vm --help\n\
  compiler-design-vm dump <program.cdbc>\n\
  compiler-design-vm run <program.cdbc>   (planned)\n\n\
Current phase: .cdbc parsing and canonical dump are implemented. Bytecode execution is not implemented in this phase.\n";

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
            eprintln!("error: command `run` is planned but not implemented in this phase");
            eprintln!();
            eprint!("{}", help_text());
            process::exit(64);
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
    fn help_mentions_dump_scope() {
        let help = help_text();
        assert!(help.contains("compiler-design-vm dump <program.cdbc>"));
        assert!(help.contains("canonical dump are implemented"));
        assert!(help.contains("Bytecode execution is not implemented"));
    }
}
