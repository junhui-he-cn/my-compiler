use std::env;
use std::process;

const HELP: &str = "compiler-design-vm 0.1.0\n\n\
Usage:\n\
  compiler-design-vm --help\n\
  compiler-design-vm run <program.cdbc>   (planned)\n\
  compiler-design-vm dump <program.cdbc>  (planned)\n\n\
Phase 0: CLI skeleton only. .cdbc parsing and bytecode execution are not implemented in this phase.\n";

fn help_text() -> &'static str {
    HELP
}

fn main() {
    let mut args = env::args().skip(1);
    match args.next().as_deref() {
        None | Some("-h") | Some("--help") => {
            print!("{}", help_text());
        }
        Some(command) => {
            eprintln!("error: command `{}` is planned but not implemented in Phase 0", command);
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
    fn help_mentions_phase_zero_scope() {
        let help = help_text();
        assert!(help.contains("Phase 0: CLI skeleton only"));
        assert!(help.contains(".cdbc parsing and bytecode execution are not implemented"));
    }

    #[test]
    fn help_mentions_future_run_and_dump_commands() {
        let help = help_text();
        assert!(help.contains("compiler-design-vm run <program.cdbc>"));
        assert!(help.contains("compiler-design-vm dump <program.cdbc>"));
    }
}
