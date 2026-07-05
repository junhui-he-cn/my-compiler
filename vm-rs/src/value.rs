#![allow(dead_code)]

use crate::runtime::{ArrayValue, FunctionValue, StructValue};
use std::fmt;

#[derive(Clone, Debug)]
pub enum Value {
    Nil,
    Number(f64),
    Bool(bool),
    String(String),
    Function(FunctionValue),
    Array(ArrayValue),
    Struct(StructValue),
}

impl Value {
    pub fn number(value: f64) -> Self {
        Self::Number(value)
    }

    pub fn boolean(value: bool) -> Self {
        Self::Bool(value)
    }

    pub fn string(value: impl Into<String>) -> Self {
        Self::String(value.into())
    }

    pub fn function(value: FunctionValue) -> Self {
        Self::Function(value)
    }

    pub fn array(value: ArrayValue) -> Self {
        Self::Array(value)
    }

    pub fn structure(value: StructValue) -> Self {
        Self::Struct(value)
    }

    pub fn type_name(&self) -> &'static str {
        match self {
            Self::Nil => "nil",
            Self::Number(_) => "number",
            Self::Bool(_) => "bool",
            Self::String(_) => "string",
            Self::Function(_) => "function",
            Self::Array(_) => "array",
            Self::Struct(_) => "struct",
        }
    }

    pub fn is_truthy(&self) -> bool {
        !matches!(self, Self::Nil | Self::Bool(false))
    }

    pub fn runtime_equals(&self, other: &Self) -> bool {
        match (self, other) {
            (Self::Nil, Self::Nil) => true,
            (Self::Number(left), Self::Number(right)) => left == right,
            (Self::Bool(left), Self::Bool(right)) => left == right,
            (Self::String(left), Self::String(right)) => left == right,
            (Self::Function(left), Self::Function(right)) => left.identity == right.identity,
            (Self::Array(left), Self::Array(right)) => left.identity == right.identity,
            (Self::Struct(left), Self::Struct(right)) => left.identity == right.identity,
            _ => false,
        }
    }
}

fn format_number(value: f64) -> String {
    if value.fract() == 0.0 {
        format!("{:.0}", value)
    } else {
        value.to_string()
    }
}

impl fmt::Display for Value {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::Nil => write!(f, "nil"),
            Self::Number(value) => write!(f, "{}", format_number(*value)),
            Self::Bool(value) => write!(f, "{}", if *value { "true" } else { "false" }),
            Self::String(value) => write!(f, "{}", value),
            Self::Function(function) => write!(f, "<fn {}>", function.name),
            Self::Array(array) => {
                write!(f, "[")?;
                let elements = array.elements.borrow();
                for (index, value) in elements.iter().enumerate() {
                    if index != 0 {
                        write!(f, ", ")?;
                    }
                    write!(f, "{}", value)?;
                }
                write!(f, "]")
            }
            Self::Struct(value) => {
                write!(f, "{{")?;
                let fields = value.fields.borrow();
                for (index, (name, field_value)) in fields.iter().enumerate() {
                    if index != 0 {
                        write!(f, ", ")?;
                    }
                    write!(f, "{}: {}", name, field_value)?;
                }
                write!(f, "}}")
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::Value;

    #[test]
    fn formats_primitives_like_cpp_runtime() {
        assert_eq!(Value::Nil.to_string(), "nil");
        assert_eq!(Value::number(7.0).to_string(), "7");
        assert_eq!(Value::number(1.25).to_string(), "1.25");
        assert_eq!(Value::boolean(true).to_string(), "true");
        assert_eq!(Value::boolean(false).to_string(), "false");
        assert_eq!(Value::string("hello").to_string(), "hello");
    }

    #[test]
    fn truthiness_matches_language_runtime() {
        assert!(!Value::Nil.is_truthy());
        assert!(!Value::boolean(false).is_truthy());
        assert!(Value::boolean(true).is_truthy());
        assert!(Value::number(0.0).is_truthy());
        assert!(Value::string("").is_truthy());
    }

    #[test]
    fn primitive_equality_matches_runtime() {
        assert!(Value::Nil.runtime_equals(&Value::Nil));
        assert!(Value::number(2.0).runtime_equals(&Value::number(2.0)));
        assert!(!Value::number(2.0).runtime_equals(&Value::number(3.0)));
        assert!(Value::boolean(true).runtime_equals(&Value::boolean(true)));
        assert!(Value::string("x").runtime_equals(&Value::string("x")));
        assert!(!Value::string("x").runtime_equals(&Value::number(0.0)));
    }
}
