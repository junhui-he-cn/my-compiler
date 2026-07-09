use crate::bytecode::{Constant, Function, FunctionBody, Instruction, Program};
use std::fmt;

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct ParseError {
    pub line: usize,
    pub message: String,
}

impl fmt::Display for ParseError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "parse error at line {}: {}", self.line, self.message)
    }
}

struct Parser<'a> {
    lines: Vec<(usize, &'a str)>,
    current: usize,
}

impl<'a> Parser<'a> {
    fn new(source: &'a str) -> Self {
        let lines = source
            .lines()
            .enumerate()
            .filter_map(|(index, line)| {
                let trimmed = line.trim();
                if trimmed.is_empty() {
                    None
                } else {
                    Some((index + 1, trimmed))
                }
            })
            .collect();
        Self { lines, current: 0 }
    }

    fn is_at_end(&self) -> bool {
        self.current >= self.lines.len()
    }

    fn peek(&self) -> Option<(usize, &'a str)> {
        self.lines.get(self.current).copied()
    }

    fn advance(&mut self) -> Option<(usize, &'a str)> {
        let line = self.peek()?;
        self.current += 1;
        Some(line)
    }

    fn require_line(&mut self, expected: &str) -> Result<(), ParseError> {
        let (line_number, line) = self.advance().ok_or_else(|| ParseError {
            line: self.last_line(),
            message: format!("expected `{}`", expected),
        })?;
        if line == expected {
            Ok(())
        } else {
            Err(ParseError {
                line: line_number,
                message: format!("expected `{}`", expected),
            })
        }
    }

    fn last_line(&self) -> usize {
        self.lines.last().map(|(line, _)| *line).unwrap_or(1)
    }

    fn parse_constants(&mut self) -> Result<Vec<Constant>, ParseError> {
        self.require_line("constants:")?;
        let mut constants = Vec::new();
        while let Some((line_number, line)) = self.peek() {
            if line == "names:" {
                break;
            }
            self.advance();
            let (left, right) = split_once(line_number, line, " = ")?;
            let index = parse_prefixed(line_number, left, 'c', "constant reference")?;
            if index != constants.len() {
                return Err(ParseError {
                    line: line_number,
                    message: format!("expected constant c{}", constants.len()),
                });
            }
            constants.push(parse_constant(line_number, right)?);
        }
        Ok(constants)
    }

    fn parse_names(&mut self) -> Result<Vec<String>, ParseError> {
        self.require_line("names:")?;
        let mut names = Vec::new();
        while let Some((line_number, line)) = self.peek() {
            if line.starts_with("main registers=") {
                break;
            }
            self.advance();
            let (left, right) = split_once(line_number, line, " = ")?;
            let index = parse_prefixed(line_number, left, 'n', "name reference")?;
            if index != names.len() {
                return Err(ParseError {
                    line: line_number,
                    message: format!("expected name n{}", names.len()),
                });
            }
            names.push(parse_string_full(line_number, right)?);
        }
        Ok(names)
    }

    fn parse_main(&mut self) -> Result<FunctionBody, ParseError> {
        let (line_number, line) = self.advance().ok_or_else(|| ParseError {
            line: self.last_line(),
            message: "expected main section".to_string(),
        })?;
        let registers =
            parse_wrapped_usize(line_number, line, "main registers=", ":", "main section")?;
        let instructions = self.parse_instructions_until_function()?;
        Ok(FunctionBody {
            registers,
            instructions,
        })
    }

    fn parse_functions(&mut self) -> Result<Vec<Function>, ParseError> {
        let mut functions = Vec::new();
        while !self.is_at_end() {
            let (line_number, line) = self.advance().expect("checked end");
            let (index, name, arity, registers) = parse_function_header(line_number, line)?;
            if index != functions.len() {
                return Err(ParseError {
                    line: line_number,
                    message: format!("expected function f{}", functions.len()),
                });
            }

            let mut params = Vec::new();
            while let Some((param_line, candidate)) = self.peek() {
                if !candidate.starts_with("param ") {
                    break;
                }
                self.advance();
                let (param_index, param_name) = parse_param(param_line, candidate)?;
                if param_index != params.len() {
                    return Err(ParseError {
                        line: param_line,
                        message: format!("expected param {}", params.len()),
                    });
                }
                params.push(param_name);
            }
            if params.len() != arity {
                return Err(ParseError {
                    line: line_number,
                    message: format!(
                        "function f{} expected {} params, found {}",
                        index,
                        arity,
                        params.len()
                    ),
                });
            }
            let instructions = self.parse_instructions_until_function()?;
            functions.push(Function {
                index,
                name,
                arity,
                registers,
                params,
                instructions,
            });
        }
        Ok(functions)
    }

    fn parse_instructions_until_function(&mut self) -> Result<Vec<Instruction>, ParseError> {
        let mut instructions = Vec::new();
        while let Some((line_number, line)) = self.peek() {
            if line.starts_with("function ") {
                break;
            }
            self.advance();
            instructions.push(parse_instruction(line_number, line)?);
        }
        Ok(instructions)
    }
}

pub fn parse_program(source: &str) -> Result<Program, ParseError> {
    let mut parser = Parser::new(source);
    parser.require_line("cdbc 0.1")?;
    let constants = parser.parse_constants()?;
    let names = parser.parse_names()?;
    let main = parser.parse_main()?;
    let functions = parser.parse_functions()?;
    Ok(Program {
        constants,
        names,
        main,
        functions,
    })
}

pub fn format_program(program: &Program) -> String {
    let mut out = String::new();
    out.push_str("cdbc 0.1\n\n");
    out.push_str("constants:\n");
    for (index, constant) in program.constants.iter().enumerate() {
        out.push_str(&format!("  c{} = {}\n", index, format_constant(constant)));
    }
    out.push_str("\nnames:\n");
    for (index, name) in program.names.iter().enumerate() {
        out.push_str(&format!("  n{} = {}\n", index, quote_string(name)));
    }
    out.push_str(&format!("\nmain registers={}:\n", program.main.registers));
    for instruction in &program.main.instructions {
        out.push_str("  ");
        out.push_str(&format_instruction(instruction));
        out.push('\n');
    }
    for function in &program.functions {
        out.push_str(&format!(
            "\nfunction f{} name={} arity={} registers={}:\n",
            function.index,
            quote_string(&function.name),
            function.arity,
            function.registers
        ));
        for (index, param) in function.params.iter().enumerate() {
            out.push_str(&format!("  param {} = {}\n", index, quote_string(param)));
        }
        for instruction in &function.instructions {
            out.push_str("  ");
            out.push_str(&format_instruction(instruction));
            out.push('\n');
        }
    }
    out
}

fn parse_constant(line: usize, text: &str) -> Result<Constant, ParseError> {
    if text == "nil" {
        Ok(Constant::Nil)
    } else if let Some(number) = text.strip_prefix("number ") {
        if number.is_empty() {
            Err(ParseError {
                line,
                message: "expected number literal".to_string(),
            })
        } else {
            Ok(Constant::Number(number.to_string()))
        }
    } else if let Some(value) = text.strip_prefix("bool ") {
        match value {
            "true" => Ok(Constant::Bool(true)),
            "false" => Ok(Constant::Bool(false)),
            _ => Err(ParseError {
                line,
                message: "expected bool literal".to_string(),
            }),
        }
    } else if let Some(value) = text.strip_prefix("string ") {
        Ok(Constant::String(parse_string_full(line, value)?))
    } else {
        Err(ParseError {
            line,
            message: "expected constant value".to_string(),
        })
    }
}

fn format_constant(constant: &Constant) -> String {
    match constant {
        Constant::Nil => "nil".to_string(),
        Constant::Number(value) => format!("number {}", value),
        Constant::Bool(value) => format!("bool {}", if *value { "true" } else { "false" }),
        Constant::String(value) => format!("string {}", quote_string(value)),
    }
}

fn parse_instruction(line: usize, text: &str) -> Result<Instruction, ParseError> {
    if let Some((dest_text, rest)) = text.split_once(" = ") {
        let dest = parse_register(line, dest_text)?;
        let (opcode, operands) = split_opcode(rest);
        match opcode {
            "constant" => Ok(Instruction::Constant {
                dest,
                constant: parse_constant_ref(line, operands)?,
            }),
            "make_function" => Ok(Instruction::MakeFunction {
                dest,
                function: parse_function_ref(line, operands)?,
            }),
            "array" => Ok(Instruction::Array {
                dest,
                elements: parse_register_list(line, operands)?,
            }),
            "struct" => Ok(Instruction::Struct {
                dest,
                fields: parse_struct_fields(line, operands)?,
            }),
            "move" => Ok(Instruction::Move {
                dest,
                source: parse_register(line, operands)?,
            }),
            "load_var" => Ok(Instruction::LoadVar {
                dest,
                name: parse_name_ref(line, operands)?,
            }),
            "call" => {
                let (callee, args) = split_once(line, operands, " ")?;
                Ok(Instruction::Call {
                    dest,
                    callee: parse_register(line, callee)?,
                    arguments: parse_register_list(line, args)?,
                })
            }
            "native_call" => {
                let (name, args) = split_once(line, operands, " ")?;
                Ok(Instruction::NativeCall {
                    dest,
                    name: parse_name_ref(line, name)?,
                    arguments: parse_register_list(line, args)?,
                })
            }
            "index" => {
                let (collection, index) = parse_two_registers(line, operands)?;
                Ok(Instruction::Index {
                    dest,
                    collection,
                    index,
                })
            }
            "assign_index" => {
                let parts = split_comma_parts(operands);
                if parts.len() != 3 {
                    return Err(ParseError {
                        line,
                        message: "assign_index expects three operands".to_string(),
                    });
                }
                Ok(Instruction::AssignIndex {
                    dest,
                    collection: parse_register(line, parts[0])?,
                    index: parse_register(line, parts[1])?,
                    value: parse_register(line, parts[2])?,
                })
            }
            "field" => {
                let (object, name) = split_once(line, operands, ", ")?;
                Ok(Instruction::Field {
                    dest,
                    object: parse_register(line, object)?,
                    name: parse_name_ref(line, name)?,
                })
            }
            "assign_field" => {
                let parts = split_comma_parts(operands);
                if parts.len() != 3 {
                    return Err(ParseError {
                        line,
                        message: "assign_field expects three operands".to_string(),
                    });
                }
                Ok(Instruction::AssignField {
                    dest,
                    object: parse_register(line, parts[0])?,
                    name: parse_name_ref(line, parts[1])?,
                    value: parse_register(line, parts[2])?,
                })
            }
            "len" => Ok(Instruction::Len {
                dest,
                value: parse_register(line, operands)?,
            }),
            "assert_array" => Ok(Instruction::AssertArray {
                dest,
                value: parse_register(line, operands)?,
            }),
            "negate" => Ok(Instruction::Negate {
                dest,
                value: parse_register(line, operands)?,
            }),
            "not" => Ok(Instruction::Not {
                dest,
                value: parse_register(line, operands)?,
            }),
            "add" => parse_binary(line, dest, operands, "add"),
            "subtract" => parse_binary(line, dest, operands, "subtract"),
            "multiply" => parse_binary(line, dest, operands, "multiply"),
            "divide" => parse_binary(line, dest, operands, "divide"),
            "equal" => parse_binary(line, dest, operands, "equal"),
            "not_equal" => parse_binary(line, dest, operands, "not_equal"),
            "greater" => parse_binary(line, dest, operands, "greater"),
            "greater_equal" => parse_binary(line, dest, operands, "greater_equal"),
            "less" => parse_binary(line, dest, operands, "less"),
            "less_equal" => parse_binary(line, dest, operands, "less_equal"),
            unknown => Err(ParseError {
                line,
                message: format!("unknown opcode `{}`", unknown),
            }),
        }
    } else {
        let (opcode, operands) = split_opcode(text);
        match opcode {
            "store_var" => {
                let (name, value) = split_once(line, operands, ", ")?;
                Ok(Instruction::StoreVar {
                    name: parse_name_ref(line, name)?,
                    value: parse_register(line, value)?,
                })
            }
            "assign_var" => {
                let (name, value) = split_once(line, operands, ", ")?;
                Ok(Instruction::AssignVar {
                    name: parse_name_ref(line, name)?,
                    value: parse_register(line, value)?,
                })
            }
            "print" => Ok(Instruction::Print {
                value: parse_register(line, operands)?,
            }),
            "return" => Ok(Instruction::Return {
                value: parse_register(line, operands)?,
            }),
            "jump" => Ok(Instruction::Jump {
                target: parse_usize(line, operands, "jump target")?,
            }),
            "jump_if_false" => {
                let (condition, target) = split_once(line, operands, ", ")?;
                Ok(Instruction::JumpIfFalse {
                    condition: parse_register(line, condition)?,
                    target: parse_usize(line, target, "jump target")?,
                })
            }
            "jump_if_true" => {
                let (condition, target) = split_once(line, operands, ", ")?;
                Ok(Instruction::JumpIfTrue {
                    condition: parse_register(line, condition)?,
                    target: parse_usize(line, target, "jump target")?,
                })
            }
            unknown => Err(ParseError {
                line,
                message: format!("unknown opcode `{}`", unknown),
            }),
        }
    }
}

fn format_instruction(instruction: &Instruction) -> String {
    match instruction {
        Instruction::Constant { dest, constant } => format!("r{} = constant c{}", dest, constant),
        Instruction::MakeFunction { dest, function } => {
            format!("r{} = make_function f{}", dest, function)
        }
        Instruction::Array { dest, elements } => {
            format!("r{} = array {}", dest, format_register_list(elements))
        }
        Instruction::Struct { dest, fields } => {
            let parts = fields
                .iter()
                .map(|(name, value)| format!("n{}: r{}", name, value))
                .collect::<Vec<_>>()
                .join(", ");
            format!("r{} = struct {{{}}}", dest, parts)
        }
        Instruction::Move { dest, source } => format!("r{} = move r{}", dest, source),
        Instruction::LoadVar { dest, name } => format!("r{} = load_var n{}", dest, name),
        Instruction::StoreVar { name, value } => format!("store_var n{}, r{}", name, value),
        Instruction::AssignVar { name, value } => format!("assign_var n{}, r{}", name, value),
        Instruction::Call {
            dest,
            callee,
            arguments,
        } => format!(
            "r{} = call r{} {}",
            dest,
            callee,
            format_register_list(arguments)
        ),
        Instruction::NativeCall {
            dest,
            name,
            arguments,
        } => format!(
            "r{} = native_call n{} {}",
            dest,
            name,
            format_register_list(arguments)
        ),
        Instruction::Index {
            dest,
            collection,
            index,
        } => format!("r{} = index r{}, r{}", dest, collection, index),
        Instruction::AssignIndex {
            dest,
            collection,
            index,
            value,
        } => format!(
            "r{} = assign_index r{}, r{}, r{}",
            dest, collection, index, value
        ),
        Instruction::Field { dest, object, name } => {
            format!("r{} = field r{}, n{}", dest, object, name)
        }
        Instruction::AssignField {
            dest,
            object,
            name,
            value,
        } => format!("r{} = assign_field r{}, n{}, r{}", dest, object, name, value),
        Instruction::Len { dest, value } => format!("r{} = len r{}", dest, value),
        Instruction::AssertArray { dest, value } => format!("r{} = assert_array r{}", dest, value),
        Instruction::Print { value } => format!("print r{}", value),
        Instruction::Return { value } => format!("return r{}", value),
        Instruction::Negate { dest, value } => format!("r{} = negate r{}", dest, value),
        Instruction::Not { dest, value } => format!("r{} = not r{}", dest, value),
        Instruction::Add { dest, left, right } => format!("r{} = add r{}, r{}", dest, left, right),
        Instruction::Subtract { dest, left, right } => {
            format!("r{} = subtract r{}, r{}", dest, left, right)
        }
        Instruction::Multiply { dest, left, right } => {
            format!("r{} = multiply r{}, r{}", dest, left, right)
        }
        Instruction::Divide { dest, left, right } => {
            format!("r{} = divide r{}, r{}", dest, left, right)
        }
        Instruction::Equal { dest, left, right } => {
            format!("r{} = equal r{}, r{}", dest, left, right)
        }
        Instruction::NotEqual { dest, left, right } => {
            format!("r{} = not_equal r{}, r{}", dest, left, right)
        }
        Instruction::Greater { dest, left, right } => {
            format!("r{} = greater r{}, r{}", dest, left, right)
        }
        Instruction::GreaterEqual { dest, left, right } => {
            format!("r{} = greater_equal r{}, r{}", dest, left, right)
        }
        Instruction::Less { dest, left, right } => {
            format!("r{} = less r{}, r{}", dest, left, right)
        }
        Instruction::LessEqual { dest, left, right } => {
            format!("r{} = less_equal r{}, r{}", dest, left, right)
        }
        Instruction::Jump { target } => format!("jump {}", target),
        Instruction::JumpIfFalse { condition, target } => {
            format!("jump_if_false r{}, {}", condition, target)
        }
        Instruction::JumpIfTrue { condition, target } => {
            format!("jump_if_true r{}, {}", condition, target)
        }
    }
}

fn parse_binary(
    line: usize,
    dest: usize,
    operands: &str,
    opcode: &str,
) -> Result<Instruction, ParseError> {
    let (left, right) = parse_two_registers(line, operands)?;
    match opcode {
        "add" => Ok(Instruction::Add { dest, left, right }),
        "subtract" => Ok(Instruction::Subtract { dest, left, right }),
        "multiply" => Ok(Instruction::Multiply { dest, left, right }),
        "divide" => Ok(Instruction::Divide { dest, left, right }),
        "equal" => Ok(Instruction::Equal { dest, left, right }),
        "not_equal" => Ok(Instruction::NotEqual { dest, left, right }),
        "greater" => Ok(Instruction::Greater { dest, left, right }),
        "greater_equal" => Ok(Instruction::GreaterEqual { dest, left, right }),
        "less" => Ok(Instruction::Less { dest, left, right }),
        "less_equal" => Ok(Instruction::LessEqual { dest, left, right }),
        _ => unreachable!("validated binary opcode"),
    }
}

fn parse_two_registers(line: usize, text: &str) -> Result<(usize, usize), ParseError> {
    let (left, right) = split_once(line, text, ", ")?;
    Ok((parse_register(line, left)?, parse_register(line, right)?))
}

fn parse_register(line: usize, text: &str) -> Result<usize, ParseError> {
    parse_prefixed(line, text, 'r', "register reference")
}

fn parse_constant_ref(line: usize, text: &str) -> Result<usize, ParseError> {
    parse_prefixed(line, text, 'c', "constant reference")
}

fn parse_name_ref(line: usize, text: &str) -> Result<usize, ParseError> {
    parse_prefixed(line, text, 'n', "name reference")
}

fn parse_function_ref(line: usize, text: &str) -> Result<usize, ParseError> {
    parse_prefixed(line, text, 'f', "function reference")
}

fn parse_prefixed(
    line: usize,
    text: &str,
    prefix: char,
    description: &str,
) -> Result<usize, ParseError> {
    let Some(rest) = text.strip_prefix(prefix) else {
        return Err(ParseError {
            line,
            message: format!("expected {}", description),
        });
    };
    parse_usize(line, rest, description)
}

fn parse_usize(line: usize, text: &str, description: &str) -> Result<usize, ParseError> {
    if text.is_empty() {
        return Err(ParseError {
            line,
            message: format!("expected {}", description),
        });
    }
    text.parse::<usize>().map_err(|_| ParseError {
        line,
        message: format!("expected {}", description),
    })
}

fn parse_register_list(line: usize, text: &str) -> Result<Vec<usize>, ParseError> {
    if !text.starts_with('[') || !text.ends_with(']') {
        return Err(ParseError {
            line,
            message: "expected register list".to_string(),
        });
    }
    let inner = &text[1..text.len() - 1];
    if inner.is_empty() {
        return Ok(Vec::new());
    }
    inner
        .split(", ")
        .map(|part| parse_register(line, part))
        .collect()
}

fn parse_struct_fields(line: usize, text: &str) -> Result<Vec<(usize, usize)>, ParseError> {
    if !text.starts_with('{') || !text.ends_with('}') {
        return Err(ParseError {
            line,
            message: "struct fields must be wrapped in braces".to_string(),
        });
    }
    let inner = &text[1..text.len() - 1];
    if inner.is_empty() {
        return Ok(Vec::new());
    }
    let mut fields = Vec::new();
    for part in split_comma_parts(inner) {
        let (name, value) = split_once(line, part, ": ")?;
        fields.push((parse_name_ref(line, name)?, parse_register(line, value)?));
    }
    Ok(fields)
}

fn format_register_list(registers: &[usize]) -> String {
    let inner = registers
        .iter()
        .map(|reg| format!("r{}", reg))
        .collect::<Vec<_>>()
        .join(", ");
    format!("[{}]", inner)
}

fn split_once<'a>(
    line: usize,
    text: &'a str,
    delimiter: &str,
) -> Result<(&'a str, &'a str), ParseError> {
    text.split_once(delimiter).ok_or_else(|| ParseError {
        line,
        message: format!("expected `{}`", delimiter.trim()),
    })
}

fn split_opcode(text: &str) -> (&str, &str) {
    text.split_once(' ').unwrap_or((text, ""))
}

fn split_comma_parts(text: &str) -> Vec<&str> {
    text.split(", ").collect()
}

fn parse_wrapped_usize(
    line: usize,
    text: &str,
    prefix: &str,
    suffix: &str,
    description: &str,
) -> Result<usize, ParseError> {
    let Some(rest) = text.strip_prefix(prefix) else {
        return Err(ParseError {
            line,
            message: format!("expected {}", description),
        });
    };
    let Some(value) = rest.strip_suffix(suffix) else {
        return Err(ParseError {
            line,
            message: format!("expected {}", description),
        });
    };
    parse_usize(line, value, description)
}

fn parse_function_header(
    line: usize,
    text: &str,
) -> Result<(usize, String, usize, usize), ParseError> {
    let Some(rest) = text.strip_prefix("function ") else {
        return Err(ParseError {
            line,
            message: "expected function section".to_string(),
        });
    };
    let (function_ref, rest) = split_once(line, rest, " ")?;
    let index = parse_function_ref(line, function_ref)?;
    let Some(rest) = rest.strip_prefix("name=") else {
        return Err(ParseError {
            line,
            message: "expected function name".to_string(),
        });
    };
    let (name, rest) = parse_string_prefix(line, rest)?;
    let Some(rest) = rest.strip_prefix(" arity=") else {
        return Err(ParseError {
            line,
            message: "expected function arity".to_string(),
        });
    };
    let (arity_text, rest) = split_once(line, rest, " ")?;
    let arity = parse_usize(line, arity_text, "function arity")?;
    let registers = parse_wrapped_usize(line, rest, "registers=", ":", "function registers")?;
    Ok((index, name, arity, registers))
}

fn parse_param(line: usize, text: &str) -> Result<(usize, String), ParseError> {
    let Some(rest) = text.strip_prefix("param ") else {
        return Err(ParseError {
            line,
            message: "expected param".to_string(),
        });
    };
    let (index, value) = split_once(line, rest, " = ")?;
    Ok((
        parse_usize(line, index, "param index")?,
        parse_string_full(line, value)?,
    ))
}

fn parse_string_full(line: usize, text: &str) -> Result<String, ParseError> {
    let (value, rest) = parse_string_prefix(line, text)?;
    if rest.is_empty() {
        Ok(value)
    } else {
        Err(ParseError {
            line,
            message: "unexpected characters after string".to_string(),
        })
    }
}

fn parse_string_prefix<'a>(line: usize, text: &'a str) -> Result<(String, &'a str), ParseError> {
    let mut chars = text.char_indices();
    if chars.next().map(|(_, ch)| ch) != Some('"') {
        return Err(ParseError {
            line,
            message: "expected string literal".to_string(),
        });
    }
    let mut value = String::new();
    let mut escaped = false;
    for (index, ch) in chars {
        if escaped {
            match ch {
                '\\' => value.push('\\'),
                '"' => value.push('"'),
                'n' => value.push('\n'),
                'r' => value.push('\r'),
                't' => value.push('\t'),
                other => {
                    return Err(ParseError {
                        line,
                        message: format!("unsupported escape `\\{}`", other),
                    })
                }
            }
            escaped = false;
        } else if ch == '\\' {
            escaped = true;
        } else if ch == '"' {
            return Ok((value, &text[index + ch.len_utf8()..]));
        } else {
            value.push(ch);
        }
    }
    Err(ParseError {
        line,
        message: "unterminated string literal".to_string(),
    })
}

fn quote_string(value: &str) -> String {
    let mut out = String::from("\"");
    for ch in value.chars() {
        match ch {
            '\\' => out.push_str("\\\\"),
            '"' => out.push_str("\\\""),
            '\n' => out.push_str("\\n"),
            '\r' => out.push_str("\\r"),
            '\t' => out.push_str("\\t"),
            other => out.push(other),
        }
    }
    out.push('"');
    out
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn round_trips_minimal_program() {
        let source = "cdbc 0.1\n\nconstants:\n\nnames:\n\nmain registers=1:\n  print r0\n";
        let program = parse_program(source).expect("parse minimal program");
        assert_eq!(format_program(&program), source);
    }

    #[test]
    fn parses_and_formats_string_escapes() {
        let source = "cdbc 0.1\n\nconstants:\n  c0 = string \"a\\\\b\\\"c\\n\\r\\t\"\n\nnames:\n  n0 = \"x\\\\y\"\n\nmain registers=1:\n  r0 = constant c0\n";
        let program = parse_program(source).expect("parse escaped strings");
        assert_eq!(format_program(&program), source);
    }

    #[test]
    fn rejects_bad_header() {
        let error = parse_program("nope\n").expect_err("bad header should fail");
        assert_eq!(error.line, 1);
    }

    #[test]
    fn rejects_unknown_opcode() {
        let source = "cdbc 0.1\n\nconstants:\n\nnames:\n\nmain registers=2:\n  r0 = mystery r1\n";
        let error = parse_program(source).expect_err("unknown opcode should fail");
        assert!(error.message.contains("unknown opcode `mystery`"));
    }

    #[test]
    fn parses_all_opcode_shapes() {
        let source = "cdbc 0.1\n\nconstants:\n  c0 = nil\n  c1 = number 1.5\n  c2 = bool true\n  c3 = string \"hello\"\n\nnames:\n  n0 = \"x#0\"\n\nmain registers=36:\n  r0 = constant c0\n  r1 = make_function f0\n  r2 = array [r0, r1]\n  r3 = move r2\n  r4 = load_var n0\n  store_var n0, r4\n  assign_var n0, r4\n  r5 = call r1 [r0, r2]\n  r6 = index r2, r0\n  r7 = assign_index r2, r0, r1\n  r8 = len r2\n  print r8\n  return r8\n  r9 = negate r8\n  r10 = not r8\n  r11 = add r8, r9\n  r12 = subtract r8, r9\n  r13 = multiply r8, r9\n  r14 = divide r8, r9\n  r15 = equal r8, r9\n  r16 = not_equal r8, r9\n  r17 = greater r8, r9\n  r18 = greater_equal r8, r9\n  r19 = less r8, r9\n  r20 = less_equal r8, r9\n  jump 27\n  jump_if_false r20, 28\n  jump_if_true r20, 29\n\nfunction f0 name=\"id\" arity=1 registers=1:\n  param 0 = \"arg#0\"\n  return r0\n";
        let program = parse_program(source).expect("parse all opcode shapes");
        assert_eq!(format_program(&program), source);
    }
}
