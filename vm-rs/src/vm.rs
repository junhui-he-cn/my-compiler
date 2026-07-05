#![allow(dead_code)]

use crate::bytecode::{Constant, FunctionBody, Instruction, Program};
use crate::runtime::{
    new_cell, new_environment, ArrayValue, Cell, FunctionValue, SharedEnvironment, StructValue,
};
use crate::value::Value;
use std::cell::RefCell;
use std::fmt;
use std::rc::Rc;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct RuntimeError {
    pub message: String,
}

impl RuntimeError {
    fn new(message: impl Into<String>) -> Self {
        Self {
            message: message.into(),
        }
    }
}

impl fmt::Display for RuntimeError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "runtime error: {}", self.message)
    }
}

struct Frame {
    ip: usize,
    registers: Vec<Value>,
    locals: SharedEnvironment,
    closure: SharedEnvironment,
    is_main: bool,
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
        };
        let main = FunctionBody {
            registers: self.program.main.registers,
            instructions: self.program.main.instructions.clone(),
        };
        self.execute_body(&main, &mut frame)?;
        Ok(self.output)
    }

    fn execute_body(
        &mut self,
        body: &FunctionBody,
        frame: &mut Frame,
    ) -> Result<Option<Value>, RuntimeError> {
        frame.ip = 0;
        while frame.ip < body.instructions.len() {
            let instruction = &body.instructions[frame.ip];
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
                Instruction::Struct { dest, fields } => {
                    let value = self.make_struct(frame, fields)?;
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
                    let result = self.call_function(function, values)?;
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
                    continue;
                }
                Instruction::JumpIfFalse { condition, target } => {
                    self.validate_jump_target(*target, body.instructions.len())?;
                    if !self.read_register(frame, *condition)?.is_truthy() {
                        frame.ip = *target;
                        continue;
                    }
                }
                Instruction::JumpIfTrue { condition, target } => {
                    self.validate_jump_target(*target, body.instructions.len())?;
                    if self.read_register(frame, *condition)?.is_truthy() {
                        frame.ip = *target;
                        continue;
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
                Instruction::Return { value } => {
                    return Ok(Some(self.read_register(frame, *value)?))
                }
            }
            frame.ip += 1;
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
    ) -> Result<Value, RuntimeError> {
        let bytecode_function = self
            .program
            .functions
            .get(function.function_index)
            .ok_or_else(|| RuntimeError::new("function index out of range"))?;
        let params = bytecode_function.params.clone();
        let registers = bytecode_function.registers;
        let instructions = bytecode_function.instructions.clone();

        if arguments.len() != params.len() {
            return Err(RuntimeError::new(format!(
                "expected {} arguments but got {}",
                params.len(),
                arguments.len()
            )));
        }

        let mut frame = Frame {
            ip: 0,
            registers: vec![Value::Nil; registers],
            locals: new_environment(),
            closure: function.closure.clone(),
            is_main: false,
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
        };
        let result = self.execute_body(&body, &mut frame)?;
        Ok(result.unwrap_or(Value::Nil))
    }

    fn make_array(&mut self, elements: Vec<Value>) -> Value {
        let identity = self.next_array_identity;
        self.next_array_identity += 1;
        Value::array(ArrayValue {
            identity,
            elements: Rc::new(RefCell::new(elements)),
        })
    }

    fn make_struct(&mut self, frame: &Frame, fields: &[(usize, usize)]) -> Result<Value, RuntimeError> {
        let identity = self.next_struct_identity;
        self.next_struct_identity += 1;
        let mut values = Vec::with_capacity(fields.len());
        for (name_index, register) in fields {
            values.push((self.read_name(*name_index)?, self.read_register(frame, *register)?));
        }
        Ok(Value::structure(StructValue {
            identity,
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
            Value::String(value) => Ok(Value::number(value.len() as f64)),
            _ => Err(RuntimeError::new("len expects array or string")),
        }
    }

    fn execute_native_call(
        &self,
        name: &str,
        arguments: Vec<Value>,
    ) -> Result<Value, RuntimeError> {
        match name {
            "push" => self.execute_native_push(arguments),
            "pop" => self.execute_native_pop(arguments),
            "floor" => self.execute_native_floor(arguments),
            "ceil" => self.execute_native_ceil(arguments),
            "sqrt" => self.execute_native_sqrt(arguments),
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
