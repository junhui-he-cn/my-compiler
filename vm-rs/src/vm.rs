#![allow(dead_code)]

use crate::bytecode::{Constant, DebugLocation, DebugSource, FunctionBody, Instruction, Program};
use crate::runtime::{
    new_cell, new_environment, ArrayValue, Cell, FunctionValue, SharedEnvironment, StructValue,
};
use crate::value::Value;
use std::cell::RefCell;
use std::fmt;
use std::rc::Rc;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct StackFrame {
    pub function: String,
    pub location: Option<DebugLocation>,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct RuntimeError {
    pub message: String,
    pub location: Option<DebugLocation>,
    pub stack: Vec<StackFrame>,
    pub sources: Vec<DebugSource>,
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::bytecode::Function;

    fn empty_program() -> Program {
        Program {
            constants: Vec::new(),
            names: Vec::new(),
            main: FunctionBody {
                registers: 0,
                instructions: Vec::new(),
                locations: Vec::new(),
            },
            functions: Vec::new(),
            debug_sources: Vec::new(),
        }
    }

    fn array_elements(value: &Value) -> Vec<Value> {
        let Value::Array(array) = value else {
            panic!("expected array");
        };
        array.elements.borrow().clone()
    }

    fn debug_failure_program() -> Program {
        let source = DebugSource {
            path: "demo.cd".to_string(),
            text: "fun fail() { return 1 / 0; }\nfail();\n".to_string(),
        };
        Program {
            constants: vec![Constant::Number("1".to_string()), Constant::Number("0".to_string())],
            names: Vec::new(),
            main: FunctionBody {
                registers: 2,
                instructions: vec![
                    Instruction::MakeFunction { dest: 0, function: 0 },
                    Instruction::Call {
                        dest: 1,
                        callee: 0,
                        arguments: Vec::new(),
                    },
                ],
                locations: vec![
                    Some(DebugLocation { source: 0, line: 2, column: 1 }),
                    Some(DebugLocation { source: 0, line: 2, column: 1 }),
                ],
            },
            functions: vec![Function {
                index: 0,
                name: "fail".to_string(),
                arity: 0,
                registers: 4,
                params: Vec::new(),
                instructions: vec![
                    Instruction::Constant { dest: 0, constant: 0 },
                    Instruction::Constant { dest: 1, constant: 1 },
                    Instruction::Divide { dest: 2, left: 0, right: 1 },
                    Instruction::Return { value: 2 },
                ],
                locations: vec![
                    Some(DebugLocation { source: 0, line: 1, column: 21 }),
                    Some(DebugLocation { source: 0, line: 1, column: 25 }),
                    Some(DebugLocation { source: 0, line: 1, column: 21 }),
                    Some(DebugLocation { source: 0, line: 1, column: 14 }),
                ],
            }],
            debug_sources: vec![source],
        }
    }

    #[test]
    fn native_collection_helpers_query_and_copy_shallowly() {
        let program = empty_program();
        let mut vm = VM::new(&program);
        let shared = vm.make_array(vec![Value::number(9.0)]);
        let source = vm.make_array(vec![Value::number(1.0), shared.clone(), Value::number(3.0)]);
        let distinct = vm.make_array(vec![Value::number(9.0)]);

        let contains_shared = vm
            .execute_native_call("contains", vec![source.clone(), shared.clone()])
            .expect("contains succeeds");
        let contains_distinct = vm
            .execute_native_call("contains", vec![source.clone(), distinct])
            .expect("contains succeeds");
        assert!(matches!(contains_shared, Value::Bool(true)));
        assert!(matches!(contains_distinct, Value::Bool(false)));

        let sliced = vm
            .execute_native_call(
                "slice",
                vec![source.clone(), Value::number(1.0), Value::number(2.0)],
            )
            .expect("slice succeeds");
        let copied = vm
            .execute_native_call("copy", vec![source.clone()])
            .expect("copy succeeds");
        let concatenated = vm
            .execute_native_call("concat", vec![sliced.clone(), copied.clone()])
            .expect("concat succeeds");

        assert_eq!(array_elements(&sliced).len(), 2);
        assert_eq!(array_elements(&copied).len(), 3);
        assert_eq!(array_elements(&concatenated).len(), 5);
        assert!(!source.runtime_equals(&copied));
        let source_elements = array_elements(&source);
        let copied_elements = array_elements(&copied);
        assert!(source_elements[1].runtime_equals(&copied_elements[1]));
    }

    #[test]
    fn native_collection_helpers_validate_slice_boundaries() {
        let program = empty_program();
        let mut vm = VM::new(&program);
        let source = vm.make_array(vec![Value::number(1.0)]);

        let empty = vm
            .execute_native_call(
                "slice",
                vec![source.clone(), Value::number(1.0), Value::number(0.0)],
            )
            .expect("empty end slice succeeds");
        assert!(array_elements(&empty).is_empty());

        for (start, length, expected) in [
            (f64::NAN, 0.0, "slice expects integer start offset"),
            (-1.0, 0.0, "slice start offset out of bounds"),
            (0.0, f64::INFINITY, "slice expects integer length"),
            (0.0, 2.0, "slice length out of bounds"),
        ] {
            let error = vm
                .execute_native_call(
                    "slice",
                    vec![source.clone(), Value::number(start), Value::number(length)],
                )
                .expect_err("slice should fail");
            assert_eq!(error.message, expected);
        }
    }

    #[test]
    fn native_collection_helpers_validate_arity_and_types() {
        let program = empty_program();
        let mut vm = VM::new(&program);
        assert_eq!(
            vm.execute_native_call("contains", vec![])
                .unwrap_err()
                .message,
            "contains expects 2 arguments"
        );
        assert_eq!(
            vm.execute_native_call("slice", vec![]).unwrap_err().message,
            "slice expects 3 arguments"
        );
        assert_eq!(
            vm.execute_native_call("copy", vec![]).unwrap_err().message,
            "copy expects 1 argument"
        );
        assert_eq!(
            vm.execute_native_call("concat", vec![])
                .unwrap_err()
                .message,
            "concat expects 2 arguments"
        );
        assert_eq!(
            vm.execute_native_call("copy", vec![Value::number(1.0)])
                .unwrap_err()
                .message,
            "copy expects array as first argument"
        );
        assert_eq!(
            vm.execute_native_call("concat", vec![Value::Nil, Value::Nil])
                .unwrap_err()
                .message,
            "concat expects array as first argument"
        );
    }

    #[test]
    fn native_string_helpers_use_unicode_scalar_boundaries() {
        let program = empty_program();
        let mut vm = VM::new(&program);
        let source = Value::string("你🙂e\u{301}");

        let length = vm.execute_len(source.clone()).expect("len succeeds");
        assert!(matches!(length, Value::Number(value) if value == 4.0));

        let sliced = vm
            .execute_native_call(
                "substr",
                vec![source.clone(), Value::number(1.0), Value::number(2.0)],
            )
            .expect("substr succeeds");
        assert!(matches!(sliced, Value::String(value) if value == "🙂e"));

        let combined = vm
            .execute_native_call(
                "substr",
                vec![source.clone(), Value::number(2.0), Value::number(2.0)],
            )
            .expect("combining scalar slice succeeds");
        assert!(matches!(combined, Value::String(value) if value == "e\u{301}"));

        let character = vm
            .execute_native_call("charAt", vec![source, Value::number(1.0)])
            .expect("charAt succeeds");
        assert!(matches!(character, Value::String(value) if value == "🙂"));
    }

    #[test]
    fn native_string_helpers_validate_scalar_boundaries() {
        let program = empty_program();
        let mut vm = VM::new(&program);
        let source = Value::string("你🙂");

        let empty = vm
            .execute_native_call(
                "substr",
                vec![source.clone(), Value::number(2.0), Value::number(0.0)],
            )
            .expect("empty end slice succeeds");
        assert!(matches!(empty, Value::String(value) if value.is_empty()));

        for (start, length, expected) in [
            (3.0, 0.0, "substr start offset out of bounds"),
            (1.0, 2.0, "substr length out of bounds"),
            (-1.0, 0.0, "substr start offset out of bounds"),
            (1.5, 0.0, "substr expects integer start offset"),
        ] {
            let error = vm
                .execute_native_call(
                    "substr",
                    vec![source.clone(), Value::number(start), Value::number(length)],
                )
                .expect_err("substr should fail");
            assert_eq!(error.message, expected);
        }

        for (index, expected) in [
            (2.0, "charAt index out of bounds"),
            (1.5, "charAt expects integer index"),
        ] {
            let error = vm
                .execute_native_call("charAt", vec![source.clone(), Value::number(index)])
                .expect_err("charAt should fail");
            assert_eq!(error.message, expected);
        }
    }

    #[test]
    fn runtime_error_reports_inner_location_then_outer_call_site() {
        let error = VM::new(&debug_failure_program()).run().unwrap_err();
        assert_eq!(error.location.as_ref().unwrap().line, 1);
        assert_eq!(error.stack[0].function, "fail");
        assert_eq!(error.stack[1].function, "main");
        assert!(error.to_string().contains("Call stack:\n"));
        assert!(error.to_string().contains("  fun fail() { return 1 / 0; }\n"));
    }

    #[test]
    fn native_failure_uses_native_call_location() {
        let program = Program {
            constants: vec![Constant::Nil],
            names: vec!["sqrt".to_string()],
            main: FunctionBody {
                registers: 2,
                instructions: vec![
                    Instruction::Constant { dest: 0, constant: 0 },
                    Instruction::NativeCall {
                        dest: 1,
                        name: 0,
                        arguments: vec![0],
                    },
                ],
                locations: vec![
                    Some(DebugLocation { source: 0, line: 1, column: 7 }),
                    Some(DebugLocation { source: 0, line: 1, column: 1 }),
                ],
            },
            functions: Vec::new(),
            debug_sources: vec![DebugSource {
                path: "native.cd".to_string(),
                text: "sqrt(nil);\n".to_string(),
            }],
        };
        let error = VM::new(&program).run().unwrap_err();
        assert_eq!(error.location.as_ref().unwrap().column, 1);
        assert!(error.to_string().contains("sqrt(nil);"));
    }

    #[test]
    fn metadata_free_runtime_errors_keep_legacy_format() {
        let error = VM::new(&empty_program()).execute_native_call("sqrt", vec![Value::Nil]).unwrap_err();
        assert_eq!(error.to_string(), "Runtime error: sqrt expects number");
    }

    #[test]
    fn invalid_debug_source_lookup_does_not_panic() {
        let mut program = debug_failure_program();
        program.functions[0].locations[2] = Some(DebugLocation { source: 99, line: 1, column: 1 });
        let error = VM::new(&program).run().unwrap_err();
        assert!(error.to_string().starts_with("Runtime error: "));
    }
}

impl RuntimeError {
    fn new(message: impl Into<String>) -> Self {
        Self {
            message: message.into(),
            location: None,
            stack: Vec::new(),
            sources: Vec::new(),
        }
    }

    fn push_frame(&mut self, function: String, location: Option<DebugLocation>) {
        self.stack.push(StackFrame { function, location });
    }

    fn source_location(&self, location: &DebugLocation) -> Option<(&DebugSource, &str)> {
        let source = self.sources.get(location.source)?;
        if location.line == 0 || location.column == 0 {
            return None;
        }
        let line = source.text.split('\n').nth(location.line - 1)?;
        let line = line.strip_suffix('\r').unwrap_or(line);
        if location.column > line.len() + 1 {
            return None;
        }
        Some((source, line))
    }
}

impl fmt::Display for RuntimeError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let mut output = String::new();
        if let Some(location) = self.location.as_ref() {
            if let Some((source, line)) = self.source_location(location) {
                output.push_str(&format!(
                    "Runtime error at {}:{}:{}: {}\n",
                    source.path, location.line, location.column, self.message
                ));
                output.push_str(&format!("  {}\n", line));
                output.push_str(&format!(
                    "  {}^",
                    " ".repeat(location.column.saturating_sub(1))
                ));
            } else {
                output.push_str(&format!("Runtime error: {}", self.message));
            }
        } else {
            output.push_str(&format!("Runtime error: {}", self.message));
        }

        let valid_frames = self
            .stack
            .iter()
            .filter_map(|frame| frame.location.as_ref().map(|location| (frame, location)))
            .filter(|(_, location)| self.source_location(location).is_some())
            .collect::<Vec<_>>();
        if !valid_frames.is_empty() {
            output.push_str("\nCall stack:\n");
            for (index, (frame, location)) in valid_frames.iter().enumerate() {
                let source = self
                    .sources
                    .get(location.source)
                    .expect("validated debug source");
                if index != 0 {
                    output.push('\n');
                }
                output.push_str(&format!(
                    "  at {} ({}:{}:{})",
                    frame.function, source.path, location.line, location.column
                ));
            }
        }
        write!(f, "{}", output)
    }
}

struct Frame {
    ip: usize,
    registers: Vec<Value>,
    locals: SharedEnvironment,
    closure: SharedEnvironment,
    is_main: bool,
    function: String,
    function_index: Option<usize>,
}

pub struct VM<'a> {
    program: &'a Program,
    globals: SharedEnvironment,
    output: String,
    next_function_identity: usize,
    next_array_identity: usize,
    next_struct_identity: usize,
}

impl<'a> VM<'a> {
    pub fn new(program: &'a Program) -> Self {
        Self {
            program,
            globals: new_environment(),
            output: String::new(),
            next_function_identity: 1,
            next_array_identity: 1,
            next_struct_identity: 1,
        }
    }

    pub fn run(mut self) -> Result<String, RuntimeError> {
        let mut frame = Frame {
            ip: 0,
            registers: vec![Value::Nil; self.program.main.registers],
            locals: new_environment(),
            closure: new_environment(),
            is_main: true,
            function: "main".to_string(),
            function_index: None,
        };
        let main = FunctionBody {
            registers: self.program.main.registers,
            instructions: self.program.main.instructions.clone(),
            locations: self.program.main.locations.clone(),
        };
        match self.execute_body(&main, &mut frame) {
            Ok(_) => Ok(self.output),
            Err(mut error) => {
                if error.sources.is_empty() {
                    error.sources = self.program.debug_sources.clone();
                }
                Err(error)
            }
        }
    }

    fn execute_body(
        &mut self,
        body: &FunctionBody,
        frame: &mut Frame,
    ) -> Result<Option<Value>, RuntimeError> {
        frame.ip = 0;
        while frame.ip < body.instructions.len() {
            let instruction = &body.instructions[frame.ip];
            let mut jumped = false;
            let result = (|| -> Result<Option<Value>, RuntimeError> {
                match instruction {
                Instruction::Constant { dest, constant } => {
                    let value = self.constant_value(*constant)?;
                    self.write_register(frame, *dest, value)?;
                }
                Instruction::Print { value } => {
                    let value = self.read_register(frame, *value)?;
                    self.output.push_str(&value.to_string());
                    self.output.push('\n');
                }
                Instruction::MakeFunction { dest, function } => {
                    let value = self.make_function(*function, frame)?;
                    self.write_register(frame, *dest, value)?;
                }
                Instruction::Array { dest, elements } => {
                    let mut values = Vec::with_capacity(elements.len());
                    for element in elements {
                        values.push(self.read_register(frame, *element)?);
                    }
                    let value = self.make_array(values);
                    self.write_register(frame, *dest, value)?;
                }
                Instruction::Struct {
                    dest,
                    type_name,
                    fields,
                } => {
                    let type_name = type_name.map(|index| self.read_name(index)).transpose()?;
                    let value = self.make_struct(frame, type_name, fields)?;
                    self.write_register(frame, *dest, value)?;
                }
                Instruction::Move { dest, source } => {
                    let value = self.read_register(frame, *source)?;
                    self.write_register(frame, *dest, value)?;
                }
                Instruction::LoadVar { dest, name } => {
                    let name = self.read_name(*name)?;
                    let value = self.load_variable(frame, &name)?;
                    self.write_register(frame, *dest, value)?;
                }
                Instruction::StoreVar { name, value } => {
                    let name = self.read_name(*name)?;
                    let value = self.read_register(frame, *value)?;
                    self.store_variable(frame, name, value);
                }
                Instruction::AssignVar { name, value } => {
                    let name = self.read_name(*name)?;
                    let value = self.read_register(frame, *value)?;
                    self.assign_variable(frame, &name, value)?;
                }
                Instruction::Call {
                    dest,
                    callee,
                    arguments,
                } => {
                    let callee = self.read_register(frame, *callee)?;
                    let Value::Function(function) = callee else {
                        return Err(RuntimeError::new("can only call functions"));
                    };
                    let mut values = Vec::with_capacity(arguments.len());
                    for argument in arguments {
                        values.push(self.read_register(frame, *argument)?);
                    }
                    let call_site = body.locations.get(frame.ip).cloned().flatten();
                    let result = self.call_function(
                        function,
                        values,
                        frame.function.clone(),
                        call_site,
                    )?;
                    self.write_register(frame, *dest, result)?;
                }
                Instruction::NativeCall {
                    dest,
                    name,
                    arguments,
                } => {
                    let name = self.read_name(*name)?;
                    let mut values = Vec::with_capacity(arguments.len());
                    for argument in arguments {
                        values.push(self.read_register(frame, *argument)?);
                    }
                    let result = self.execute_native_call(&name, values)?;
                    self.write_register(frame, *dest, result)?;
                }
                Instruction::Negate { dest, value } => {
                    let input = self.expect_number(frame, *value, "negate")?;
                    self.write_register(frame, *dest, Value::number(-input))?;
                }
                Instruction::Not { dest, value } => {
                    let result = !self.read_register(frame, *value)?.is_truthy();
                    self.write_register(frame, *dest, Value::boolean(result))?;
                }
                Instruction::Add { dest, left, right } => {
                    let left_value = self.read_register(frame, *left)?;
                    let right_value = self.read_register(frame, *right)?;
                    let result = match (left_value, right_value) {
                        (Value::Number(left), Value::Number(right)) => Value::number(left + right),
                        (Value::String(left), Value::String(right)) => {
                            Value::string(format!("{}{}", left, right))
                        }
                        _ => {
                            return Err(RuntimeError::new("add expects two numbers or two strings"))
                        }
                    };
                    self.write_register(frame, *dest, result)?;
                }
                Instruction::Subtract { dest, left, right } => {
                    let (left, right) =
                        self.expect_two_numbers(frame, *left, *right, "subtract")?;
                    self.write_register(frame, *dest, Value::number(left - right))?;
                }
                Instruction::Multiply { dest, left, right } => {
                    let (left, right) =
                        self.expect_two_numbers(frame, *left, *right, "multiply")?;
                    self.write_register(frame, *dest, Value::number(left * right))?;
                }
                Instruction::Divide { dest, left, right } => {
                    let (left, right) = self.expect_two_numbers(frame, *left, *right, "divide")?;
                    if right == 0.0 {
                        return Err(RuntimeError::new("division by zero"));
                    }
                    self.write_register(frame, *dest, Value::number(left / right))?;
                }
                Instruction::Equal { dest, left, right } => {
                    let result = self
                        .read_register(frame, *left)?
                        .runtime_equals(&self.read_register(frame, *right)?);
                    self.write_register(frame, *dest, Value::boolean(result))?;
                }
                Instruction::NotEqual { dest, left, right } => {
                    let result = !self
                        .read_register(frame, *left)?
                        .runtime_equals(&self.read_register(frame, *right)?);
                    self.write_register(frame, *dest, Value::boolean(result))?;
                }
                Instruction::Greater { dest, left, right } => {
                    self.compare(frame, *dest, *left, *right, "greater", |l, r| l > r)?
                }
                Instruction::GreaterEqual { dest, left, right } => {
                    self.compare(frame, *dest, *left, *right, "greater_equal", |l, r| l >= r)?
                }
                Instruction::Less { dest, left, right } => {
                    self.compare(frame, *dest, *left, *right, "less", |l, r| l < r)?
                }
                Instruction::LessEqual { dest, left, right } => {
                    self.compare(frame, *dest, *left, *right, "less_equal", |l, r| l <= r)?
                }
                Instruction::Jump { target } => {
                    self.validate_jump_target(*target, body.instructions.len())?;
                    frame.ip = *target;
                    jumped = true;
                    return Ok(None);
                }
                Instruction::JumpIfFalse { condition, target } => {
                    self.validate_jump_target(*target, body.instructions.len())?;
                    if !self.read_register(frame, *condition)?.is_truthy() {
                        frame.ip = *target;
                        jumped = true;
                        return Ok(None);
                    }
                }
                Instruction::JumpIfTrue { condition, target } => {
                    self.validate_jump_target(*target, body.instructions.len())?;
                    if self.read_register(frame, *condition)?.is_truthy() {
                        frame.ip = *target;
                        jumped = true;
                        return Ok(None);
                    }
                }
                Instruction::Index {
                    dest,
                    collection,
                    index,
                } => {
                    let collection = self.read_register(frame, *collection)?;
                    let index = self.read_register(frame, *index)?;
                    let value = self.execute_index(collection, index)?;
                    self.write_register(frame, *dest, value)?;
                }
                Instruction::AssignIndex {
                    dest,
                    collection,
                    index,
                    value,
                } => {
                    let collection = self.read_register(frame, *collection)?;
                    let index = self.read_register(frame, *index)?;
                    let value = self.read_register(frame, *value)?;
                    let assigned = self.execute_assign_index(collection, index, value)?;
                    self.write_register(frame, *dest, assigned)?;
                }
                Instruction::Field { dest, object, name } => {
                    let object = self.read_register(frame, *object)?;
                    let name = self.read_name(*name)?;
                    let value = self.execute_field(object, &name)?;
                    self.write_register(frame, *dest, value)?;
                }
                Instruction::AssignField {
                    dest,
                    object,
                    name,
                    value,
                } => {
                    let object = self.read_register(frame, *object)?;
                    let name = self.read_name(*name)?;
                    let value = self.read_register(frame, *value)?;
                    let assigned = self.execute_assign_field(object, &name, value)?;
                    self.write_register(frame, *dest, assigned)?;
                }
                Instruction::Len { dest, value } => {
                    let value = self.read_register(frame, *value)?;
                    let length = self.execute_len(value)?;
                    self.write_register(frame, *dest, length)?;
                }
                Instruction::AssertArray { dest, value } => {
                    let input = self.read_register(frame, *value)?;
                    if !matches!(input, Value::Array(_)) {
                        return Err(RuntimeError::new("for-in expects array"));
                    }
                    self.write_register(frame, *dest, input)?;
                }
                Instruction::AssertNumber {
                    dest,
                    value,
                    message,
                } => {
                    let input = self.read_register(frame, *value)?;
                    if !matches!(input, Value::Number(_)) {
                        let message = self.read_name(*message)?;
                        return Err(RuntimeError::new(message));
                    }
                    self.write_register(frame, *dest, input)?;
                }
                Instruction::Return { value } => {
                    return Ok(Some(self.read_register(frame, *value)?))
                }
                }
                Ok(None)
            })();
            match result {
                Ok(Some(value)) => return Ok(Some(value)),
                Ok(None) => {
                    if !jumped {
                        frame.ip += 1;
                    }
                }
                Err(mut error) => {
                    let location = body.locations.get(frame.ip).cloned().flatten();
                    if error.location.is_none() {
                        error.location = location.clone();
                    }
                    if error.stack.is_empty() {
                        error.push_frame(frame.function.clone(), location);
                    }
                    return Err(error);
                }
            }
        }
        Ok(None)
    }

    fn capture_environment(&self, frame: &Frame) -> SharedEnvironment {
        let captured = new_environment();
        {
            let mut target = captured.borrow_mut();
            for (name, cell) in frame.closure.borrow().iter() {
                target.insert(name.clone(), cell.clone());
            }
            for (name, cell) in frame.locals.borrow().iter() {
                target.insert(name.clone(), cell.clone());
            }
        }
        captured
    }

    fn make_function(
        &mut self,
        function_index: usize,
        frame: &Frame,
    ) -> Result<Value, RuntimeError> {
        let function = self
            .program
            .functions
            .get(function_index)
            .ok_or_else(|| RuntimeError::new("function index out of range"))?;
        let identity = self.next_function_identity;
        self.next_function_identity += 1;
        Ok(Value::function(FunctionValue {
            name: function.name.clone(),
            function_index,
            arity: function.params.len(),
            identity,
            closure: self.capture_environment(frame),
        }))
    }

    fn call_function(
        &mut self,
        function: FunctionValue,
        arguments: Vec<Value>,
        caller: String,
        call_site: Option<DebugLocation>,
    ) -> Result<Value, RuntimeError> {
        let Some(bytecode_function) = self.program.functions.get(function.function_index) else {
            let mut error = RuntimeError::new("function index out of range");
            error.location = call_site.clone();
            error.push_frame(caller, call_site);
            return Err(error);
        };
        let params = bytecode_function.params.clone();
        let registers = bytecode_function.registers;
        let instructions = bytecode_function.instructions.clone();

        if arguments.len() != params.len() {
            let mut error = RuntimeError::new(format!(
                "expected {} arguments but got {}",
                params.len(),
                arguments.len()
            ));
            error.location = call_site.clone();
            error.push_frame(caller, call_site);
            return Err(error);
        }

        let mut frame = Frame {
            ip: 0,
            registers: vec![Value::Nil; registers],
            locals: new_environment(),
            closure: function.closure.clone(),
            is_main: false,
            function: bytecode_function.name.clone(),
            function_index: Some(function.function_index),
        };

        for (index, argument) in arguments.into_iter().enumerate() {
            frame
                .locals
                .borrow_mut()
                .insert(params[index].clone(), new_cell(argument));
        }

        let body = FunctionBody {
            registers,
            instructions,
            locations: bytecode_function.locations.clone(),
        };
        match self.execute_body(&body, &mut frame) {
            Ok(result) => Ok(result.unwrap_or(Value::Nil)),
            Err(mut error) => {
                if error.location.is_none() {
                    error.location = call_site.clone();
                }
                error.push_frame(caller, call_site);
                Err(error)
            }
        }
    }

    fn make_array(&mut self, elements: Vec<Value>) -> Value {
        let identity = self.next_array_identity;
        self.next_array_identity += 1;
        Value::array(ArrayValue {
            identity,
            elements: Rc::new(RefCell::new(elements)),
        })
    }

    fn make_struct(
        &mut self,
        frame: &Frame,
        type_name: Option<String>,
        fields: &[(usize, usize)],
    ) -> Result<Value, RuntimeError> {
        let identity = self.next_struct_identity;
        self.next_struct_identity += 1;
        let mut values = Vec::with_capacity(fields.len());
        for (name_index, register) in fields {
            values.push((
                self.read_name(*name_index)?,
                self.read_register(frame, *register)?,
            ));
        }
        Ok(Value::structure(StructValue {
            identity,
            type_name,
            fields: Rc::new(RefCell::new(values)),
        }))
    }

    fn checked_array_index(&self, index_value: Value) -> Result<usize, RuntimeError> {
        let Value::Number(number) = index_value else {
            return Err(RuntimeError::new("array index must be number"));
        };
        let integer = number.trunc();
        if integer != number {
            return Err(RuntimeError::new("array index must be integer"));
        }
        if integer < 0.0 {
            return Err(RuntimeError::new("array index out of range"));
        }
        Ok(integer as usize)
    }

    fn execute_index(&self, collection: Value, index: Value) -> Result<Value, RuntimeError> {
        let Value::Array(array) = collection else {
            return Err(RuntimeError::new("can only index arrays"));
        };
        let position = self.checked_array_index(index)?;
        let elements = array.elements.borrow();
        elements
            .get(position)
            .cloned()
            .ok_or_else(|| RuntimeError::new("array index out of range"))
    }

    fn execute_assign_index(
        &self,
        collection: Value,
        index: Value,
        value: Value,
    ) -> Result<Value, RuntimeError> {
        let Value::Array(array) = collection else {
            return Err(RuntimeError::new("can only assign array elements"));
        };
        let position = self.checked_array_index(index)?;
        let mut elements = array.elements.borrow_mut();
        if position >= elements.len() {
            return Err(RuntimeError::new("array index out of range"));
        }
        elements[position] = value.clone();
        Ok(value)
    }

    fn execute_field(&self, object: Value, name: &str) -> Result<Value, RuntimeError> {
        let Value::Struct(value) = object else {
            return Err(RuntimeError::new("can only access fields on structs"));
        };
        for (field_name, field_value) in value.fields.borrow().iter() {
            if field_name == name {
                return Ok(field_value.clone());
            }
        }
        Err(RuntimeError::new(format!("undefined field `{}`", name)))
    }

    fn execute_assign_field(
        &self,
        object: Value,
        name: &str,
        value: Value,
    ) -> Result<Value, RuntimeError> {
        let Value::Struct(struct_value) = object else {
            return Err(RuntimeError::new("can only assign fields on structs"));
        };
        let mut fields = struct_value.fields.borrow_mut();
        for (field_name, field_value) in fields.iter_mut() {
            if field_name == name {
                *field_value = value.clone();
                return Ok(value);
            }
        }
        Err(RuntimeError::new(format!("undefined field `{}`", name)))
    }

    fn execute_len(&self, value: Value) -> Result<Value, RuntimeError> {
        match value {
            Value::Array(array) => Ok(Value::number(array.elements.borrow().len() as f64)),
            Value::String(value) => Ok(Value::number(value.chars().count() as f64)),
            _ => Err(RuntimeError::new("len expects array or string")),
        }
    }

    fn execute_native_call(
        &mut self,
        name: &str,
        arguments: Vec<Value>,
    ) -> Result<Value, RuntimeError> {
        match name {
            "push" => self.execute_native_push(arguments),
            "pop" => self.execute_native_pop(arguments),
            "floor" => self.execute_native_floor(arguments),
            "ceil" => self.execute_native_ceil(arguments),
            "sqrt" => self.execute_native_sqrt(arguments),
            "str" => self.execute_native_str(arguments),
            "substr" => self.execute_native_substr(arguments),
            "charAt" => self.execute_native_char_at(arguments),
            "typeOf" => self.execute_native_type_of(arguments),
            "contains" => self.execute_native_contains(arguments),
            "slice" => self.execute_native_slice(arguments),
            "copy" => self.execute_native_copy(arguments),
            "concat" => self.execute_native_concat(arguments),
            _ => Err(RuntimeError::new(format!(
                "unknown native stdlib function `{}`",
                name
            ))),
        }
    }

    fn execute_native_push(&self, arguments: Vec<Value>) -> Result<Value, RuntimeError> {
        if arguments.len() != 2 {
            return Err(RuntimeError::new("push expects 2 arguments"));
        }
        let Value::Array(array) = &arguments[0] else {
            return Err(RuntimeError::new("push expects array as first argument"));
        };
        array.elements.borrow_mut().push(arguments[1].clone());
        Ok(Value::Nil)
    }

    fn execute_native_pop(&self, arguments: Vec<Value>) -> Result<Value, RuntimeError> {
        if arguments.len() != 1 {
            return Err(RuntimeError::new("pop expects 1 arguments"));
        }
        let Value::Array(array) = &arguments[0] else {
            return Err(RuntimeError::new("pop expects array as first argument"));
        };
        array
            .elements
            .borrow_mut()
            .pop()
            .ok_or_else(|| RuntimeError::new("cannot pop from empty array"))
    }

    fn execute_native_floor(&self, arguments: Vec<Value>) -> Result<Value, RuntimeError> {
        if arguments.len() != 1 {
            return Err(RuntimeError::new("floor expects 1 arguments"));
        }
        let Value::Number(value) = &arguments[0] else {
            return Err(RuntimeError::new("floor expects number"));
        };
        Ok(Value::number(value.floor()))
    }

    fn execute_native_ceil(&self, arguments: Vec<Value>) -> Result<Value, RuntimeError> {
        if arguments.len() != 1 {
            return Err(RuntimeError::new("ceil expects 1 arguments"));
        }
        let Value::Number(value) = &arguments[0] else {
            return Err(RuntimeError::new("ceil expects number"));
        };
        Ok(Value::number(value.ceil()))
    }

    fn execute_native_sqrt(&self, arguments: Vec<Value>) -> Result<Value, RuntimeError> {
        if arguments.len() != 1 {
            return Err(RuntimeError::new("sqrt expects 1 arguments"));
        }
        let Value::Number(value) = &arguments[0] else {
            return Err(RuntimeError::new("sqrt expects number"));
        };
        if *value < 0.0 {
            return Err(RuntimeError::new("sqrt expects non-negative number"));
        }
        Ok(Value::number(value.sqrt()))
    }

    fn checked_integer_index(
        value: f64,
        integer_message: &'static str,
        bounds_message: &'static str,
        upper_bound_inclusive: usize,
    ) -> Result<usize, RuntimeError> {
        if !value.is_finite() || value.floor() != value {
            return Err(RuntimeError::new(integer_message));
        }
        if value < 0.0 || value > upper_bound_inclusive as f64 {
            return Err(RuntimeError::new(bounds_message));
        }
        Ok(value as usize)
    }

    fn string_scalar_offsets(text: &str) -> Vec<usize> {
        let mut offsets = text
            .char_indices()
            .map(|(offset, _)| offset)
            .collect::<Vec<_>>();
        offsets.push(text.len());
        offsets
    }

    fn execute_native_str(&self, arguments: Vec<Value>) -> Result<Value, RuntimeError> {
        if arguments.len() != 1 {
            return Err(RuntimeError::new("str expects 1 arguments"));
        }
        Ok(Value::string(arguments[0].to_string()))
    }

    fn execute_native_substr(&self, arguments: Vec<Value>) -> Result<Value, RuntimeError> {
        if arguments.len() != 3 {
            return Err(RuntimeError::new("substr expects 3 arguments"));
        }
        let Value::String(text) = &arguments[0] else {
            return Err(RuntimeError::new("substr expects string as first argument"));
        };
        let Value::Number(start_value) = &arguments[1] else {
            return Err(RuntimeError::new(
                "substr expects number as second argument",
            ));
        };
        let Value::Number(length_value) = &arguments[2] else {
            return Err(RuntimeError::new("substr expects number as third argument"));
        };

        let offsets = Self::string_scalar_offsets(text);
        let scalar_count = offsets.len() - 1;
        let start = Self::checked_integer_index(
            *start_value,
            "substr expects integer start offset",
            "substr start offset out of bounds",
            scalar_count,
        )?;
        let length = Self::checked_integer_index(
            *length_value,
            "substr expects integer length",
            "substr length out of bounds",
            scalar_count,
        )?;
        if length > scalar_count - start {
            return Err(RuntimeError::new("substr length out of bounds"));
        }
        let begin = offsets[start];
        let end = offsets[start + length];
        Ok(Value::string(text[begin..end].to_string()))
    }

    fn execute_native_char_at(&self, arguments: Vec<Value>) -> Result<Value, RuntimeError> {
        if arguments.len() != 2 {
            return Err(RuntimeError::new("charAt expects 2 arguments"));
        }
        let Value::String(text) = &arguments[0] else {
            return Err(RuntimeError::new("charAt expects string as first argument"));
        };
        let Value::Number(index_value) = &arguments[1] else {
            return Err(RuntimeError::new(
                "charAt expects number as second argument",
            ));
        };
        let offsets = Self::string_scalar_offsets(text);
        let scalar_count = offsets.len() - 1;
        if scalar_count == 0 {
            return Err(RuntimeError::new("charAt index out of bounds"));
        }
        let index = Self::checked_integer_index(
            *index_value,
            "charAt expects integer index",
            "charAt index out of bounds",
            scalar_count - 1,
        )?;
        let begin = offsets[index];
        let end = offsets[index + 1];
        Ok(Value::string(text[begin..end].to_string()))
    }

    fn execute_native_type_of(&self, arguments: Vec<Value>) -> Result<Value, RuntimeError> {
        if arguments.len() != 1 {
            return Err(RuntimeError::new("typeOf expects 1 arguments"));
        }
        Ok(Value::string(arguments[0].type_name()))
    }

    fn execute_native_contains(&self, arguments: Vec<Value>) -> Result<Value, RuntimeError> {
        if arguments.len() != 2 {
            return Err(RuntimeError::new("contains expects 2 arguments"));
        }
        let Value::Array(array) = &arguments[0] else {
            return Err(RuntimeError::new(
                "contains expects array as first argument",
            ));
        };
        let found = array
            .elements
            .borrow()
            .iter()
            .any(|element| element.runtime_equals(&arguments[1]));
        Ok(Value::boolean(found))
    }

    fn execute_native_slice(&mut self, arguments: Vec<Value>) -> Result<Value, RuntimeError> {
        if arguments.len() != 3 {
            return Err(RuntimeError::new("slice expects 3 arguments"));
        }
        let Value::Array(array) = &arguments[0] else {
            return Err(RuntimeError::new("slice expects array as first argument"));
        };
        let Value::Number(start_value) = &arguments[1] else {
            return Err(RuntimeError::new("slice expects number as second argument"));
        };
        let Value::Number(length_value) = &arguments[2] else {
            return Err(RuntimeError::new("slice expects number as third argument"));
        };

        let source_len = array.elements.borrow().len();
        let start = Self::checked_integer_index(
            *start_value,
            "slice expects integer start offset",
            "slice start offset out of bounds",
            source_len,
        )?;
        let length = Self::checked_integer_index(
            *length_value,
            "slice expects integer length",
            "slice length out of bounds",
            source_len,
        )?;
        if length > source_len - start {
            return Err(RuntimeError::new("slice length out of bounds"));
        }
        let elements = array.elements.borrow()[start..start + length].to_vec();
        Ok(self.make_array(elements))
    }

    fn execute_native_copy(&mut self, arguments: Vec<Value>) -> Result<Value, RuntimeError> {
        if arguments.len() != 1 {
            return Err(RuntimeError::new("copy expects 1 argument"));
        }
        let Value::Array(array) = &arguments[0] else {
            return Err(RuntimeError::new("copy expects array as first argument"));
        };
        let elements = array.elements.borrow().clone();
        Ok(self.make_array(elements))
    }

    fn execute_native_concat(&mut self, arguments: Vec<Value>) -> Result<Value, RuntimeError> {
        if arguments.len() != 2 {
            return Err(RuntimeError::new("concat expects 2 arguments"));
        }
        let Value::Array(left) = &arguments[0] else {
            return Err(RuntimeError::new("concat expects array as first argument"));
        };
        let Value::Array(right) = &arguments[1] else {
            return Err(RuntimeError::new("concat expects array as second argument"));
        };
        let mut elements = left.elements.borrow().clone();
        elements.extend(right.elements.borrow().iter().cloned());
        Ok(self.make_array(elements))
    }

    fn read_name(&self, index: usize) -> Result<String, RuntimeError> {
        self.program
            .names
            .get(index)
            .cloned()
            .ok_or_else(|| RuntimeError::new("name index out of range"))
    }

    fn find_cell(&self, frame: &Frame, name: &str) -> Option<Cell> {
        if let Some(cell) = frame.locals.borrow().get(name) {
            return Some(cell.clone());
        }
        if let Some(cell) = frame.closure.borrow().get(name) {
            return Some(cell.clone());
        }
        self.globals.borrow().get(name).cloned()
    }

    fn load_variable(&self, frame: &Frame, name: &str) -> Result<Value, RuntimeError> {
        let cell = self
            .find_cell(frame, name)
            .ok_or_else(|| RuntimeError::new(format!("undefined variable `{}`", name)))?;
        let value = cell.borrow().clone();
        Ok(value)
    }

    fn store_variable(&self, frame: &mut Frame, name: String, value: Value) {
        let cell = new_cell(value);
        if frame.is_main {
            self.globals.borrow_mut().insert(name, cell);
        } else {
            frame.locals.borrow_mut().insert(name, cell);
        }
    }

    fn assign_variable(&self, frame: &Frame, name: &str, value: Value) -> Result<(), RuntimeError> {
        let cell = self
            .find_cell(frame, name)
            .ok_or_else(|| RuntimeError::new(format!("undefined variable `{}`", name)))?;
        *cell.borrow_mut() = value;
        Ok(())
    }

    fn validate_jump_target(
        &self,
        target: usize,
        instruction_count: usize,
    ) -> Result<(), RuntimeError> {
        if target > instruction_count {
            Err(RuntimeError::new("jump target out of range"))
        } else {
            Ok(())
        }
    }

    fn constant_value(&self, index: usize) -> Result<Value, RuntimeError> {
        let constant = self
            .program
            .constants
            .get(index)
            .ok_or_else(|| RuntimeError::new("constant index out of range"))?;
        match constant {
            Constant::Nil => Ok(Value::Nil),
            Constant::Number(value) => value
                .parse::<f64>()
                .map(Value::number)
                .map_err(|_| RuntimeError::new("invalid number constant")),
            Constant::Bool(value) => Ok(Value::boolean(*value)),
            Constant::String(value) => Ok(Value::string(value.clone())),
        }
    }

    fn read_register(&self, frame: &Frame, index: usize) -> Result<Value, RuntimeError> {
        frame
            .registers
            .get(index)
            .cloned()
            .ok_or_else(|| RuntimeError::new("register index out of range"))
    }

    fn write_register(
        &self,
        frame: &mut Frame,
        index: usize,
        value: Value,
    ) -> Result<(), RuntimeError> {
        let slot = frame
            .registers
            .get_mut(index)
            .ok_or_else(|| RuntimeError::new("register index out of range"))?;
        *slot = value;
        Ok(())
    }

    fn expect_number(
        &self,
        frame: &Frame,
        value: usize,
        op_name: &str,
    ) -> Result<f64, RuntimeError> {
        match self.read_register(frame, value)? {
            Value::Number(value) => Ok(value),
            other => Err(RuntimeError::new(format!(
                "{} expects number, got {}",
                op_name,
                other.type_name()
            ))),
        }
    }

    fn expect_two_numbers(
        &self,
        frame: &Frame,
        left: usize,
        right: usize,
        op_name: &str,
    ) -> Result<(f64, f64), RuntimeError> {
        match (
            self.read_register(frame, left)?,
            self.read_register(frame, right)?,
        ) {
            (Value::Number(left), Value::Number(right)) => Ok((left, right)),
            _ => Err(RuntimeError::new(format!("{} expects numbers", op_name))),
        }
    }

    fn compare(
        &self,
        frame: &mut Frame,
        dest: usize,
        left: usize,
        right: usize,
        op_name: &str,
        operation: fn(f64, f64) -> bool,
    ) -> Result<(), RuntimeError> {
        let (left, right) = self.expect_two_numbers(frame, left, right, op_name)?;
        self.write_register(frame, dest, Value::boolean(operation(left, right)))
    }
}
