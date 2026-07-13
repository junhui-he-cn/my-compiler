#[derive(Clone, Debug, PartialEq)]
pub struct Program {
    pub constants: Vec<Constant>,
    pub names: Vec<String>,
    pub main: FunctionBody,
    pub functions: Vec<Function>,
}

#[derive(Clone, Debug, PartialEq)]
pub enum Constant {
    Nil,
    Number(String),
    Bool(bool),
    String(String),
}

#[derive(Clone, Debug, PartialEq)]
pub struct Function {
    pub index: usize,
    pub name: String,
    pub arity: usize,
    pub registers: usize,
    pub params: Vec<String>,
    pub instructions: Vec<Instruction>,
}

#[derive(Clone, Debug, PartialEq)]
pub struct FunctionBody {
    pub registers: usize,
    pub instructions: Vec<Instruction>,
}

#[derive(Clone, Debug, PartialEq)]
pub enum Instruction {
    Constant {
        dest: usize,
        constant: usize,
    },
    MakeFunction {
        dest: usize,
        function: usize,
    },
    Array {
        dest: usize,
        elements: Vec<usize>,
    },
    Struct {
        dest: usize,
        type_name: Option<usize>,
        fields: Vec<(usize, usize)>,
    },
    Move {
        dest: usize,
        source: usize,
    },
    LoadVar {
        dest: usize,
        name: usize,
    },
    StoreVar {
        name: usize,
        value: usize,
    },
    AssignVar {
        name: usize,
        value: usize,
    },
    Call {
        dest: usize,
        callee: usize,
        arguments: Vec<usize>,
    },
    NativeCall {
        dest: usize,
        name: usize,
        arguments: Vec<usize>,
    },
    Index {
        dest: usize,
        collection: usize,
        index: usize,
    },
    AssignIndex {
        dest: usize,
        collection: usize,
        index: usize,
        value: usize,
    },
    Field {
        dest: usize,
        object: usize,
        name: usize,
    },
    AssignField {
        dest: usize,
        object: usize,
        name: usize,
        value: usize,
    },
    Len {
        dest: usize,
        value: usize,
    },
    AssertArray {
        dest: usize,
        value: usize,
    },
    AssertNumber {
        dest: usize,
        value: usize,
        message: usize,
    },
    Print {
        value: usize,
    },
    Return {
        value: usize,
    },
    Negate {
        dest: usize,
        value: usize,
    },
    Not {
        dest: usize,
        value: usize,
    },
    Add {
        dest: usize,
        left: usize,
        right: usize,
    },
    Subtract {
        dest: usize,
        left: usize,
        right: usize,
    },
    Multiply {
        dest: usize,
        left: usize,
        right: usize,
    },
    Divide {
        dest: usize,
        left: usize,
        right: usize,
    },
    Equal {
        dest: usize,
        left: usize,
        right: usize,
    },
    NotEqual {
        dest: usize,
        left: usize,
        right: usize,
    },
    Greater {
        dest: usize,
        left: usize,
        right: usize,
    },
    GreaterEqual {
        dest: usize,
        left: usize,
        right: usize,
    },
    Less {
        dest: usize,
        left: usize,
        right: usize,
    },
    LessEqual {
        dest: usize,
        left: usize,
        right: usize,
    },
    Jump {
        target: usize,
    },
    JumpIfFalse {
        condition: usize,
        target: usize,
    },
    JumpIfTrue {
        condition: usize,
        target: usize,
    },
}
