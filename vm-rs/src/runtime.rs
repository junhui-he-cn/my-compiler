#![allow(dead_code)]

use crate::value::Value;
use std::cell::RefCell;
use std::collections::HashMap;
use std::rc::Rc;

pub type Cell = Rc<RefCell<Value>>;
pub type Environment = HashMap<String, Cell>;
pub type SharedEnvironment = Rc<RefCell<Environment>>;
pub type SharedArrayElements = Rc<RefCell<Vec<Value>>>;

#[derive(Clone, Debug)]
pub struct FunctionValue {
    pub name: String,
    pub function_index: usize,
    pub arity: usize,
    pub identity: usize,
    pub closure: SharedEnvironment,
}

#[derive(Clone, Debug)]
pub struct ArrayValue {
    pub identity: usize,
    pub elements: SharedArrayElements,
}

pub fn new_environment() -> SharedEnvironment {
    Rc::new(RefCell::new(HashMap::new()))
}

pub fn new_cell(value: Value) -> Cell {
    Rc::new(RefCell::new(value))
}
