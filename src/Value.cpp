#include "Value.hpp"

#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <utility>

Value::Value(Type type)
    : type_(type)
{
}

Value Value::nil()
{
    return Value(Type::Nil);
}

Value Value::number(double value)
{
    Value result(Type::Number);
    result.number_ = value;
    return result;
}

Value Value::boolean(bool value)
{
    Value result(Type::Bool);
    result.boolean_ = value;
    return result;
}

Value Value::string(std::string value)
{
    Value result(Type::String);
    result.string_ = std::move(value);
    return result;
}

Value Value::function(FunctionValue value)
{
    Value result(Type::Function);
    result.function_ = std::move(value);
    return result;
}

Value Value::array(ArrayValue value)
{
    Value result(Type::Array);
    result.array_ = std::make_shared<ArrayValue>(std::move(value));
    return result;
}

Value::Type Value::type() const
{
    return type_;
}

double Value::asNumber() const
{
    if (type_ != Type::Number) {
        throw std::runtime_error("value is not a number");
    }
    return number_;
}

bool Value::asBool() const
{
    if (type_ != Type::Bool) {
        throw std::runtime_error("value is not a bool");
    }
    return boolean_;
}

const std::string& Value::asString() const
{
    if (type_ != Type::String) {
        throw std::runtime_error("value is not a string");
    }
    return string_;
}

const FunctionValue& Value::asFunction() const
{
    if (type_ != Type::Function) {
        throw std::runtime_error("value is not a function");
    }
    return function_;
}

const ArrayValue& Value::asArray() const
{
    if (type_ != Type::Array || !array_) {
        throw std::runtime_error("value is not an array");
    }
    return *array_;
}

bool isTruthy(const Value& value)
{
    if (value.type() == Value::Type::Nil) {
        return false;
    }
    if (value.type() == Value::Type::Bool) {
        return value.asBool();
    }
    return true;
}

bool valuesEqual(const Value& left, const Value& right)
{
    if (left.type() != right.type()) {
        return false;
    }

    switch (left.type()) {
    case Value::Type::Nil:
        return true;
    case Value::Type::Number:
        return left.asNumber() == right.asNumber();
    case Value::Type::Bool:
        return left.asBool() == right.asBool();
    case Value::Type::String:
        return left.asString() == right.asString();
    case Value::Type::Function:
        return left.asFunction().identity == right.asFunction().identity;
    case Value::Type::Array:
        return left.asArray().identity == right.asArray().identity;
    }

    return false;
}

std::string valueToString(const Value& value)
{
    switch (value.type()) {
    case Value::Type::Nil:
        return "nil";
    case Value::Type::Number: {
        std::ostringstream out;
        out << std::setprecision(15) << value.asNumber();
        return out.str();
    }
    case Value::Type::Bool:
        return value.asBool() ? "true" : "false";
    case Value::Type::String:
        return value.asString();
    case Value::Type::Function:
        return "<fun " + value.asFunction().name + ">";
    case Value::Type::Array: {
        std::ostringstream out;
        out << '[';
        const auto& elements = *value.asArray().elements;
        for (std::size_t i = 0; i < elements.size(); ++i) {
            if (i != 0) {
                out << ", ";
            }
            out << valueToString(elements[i]);
        }
        out << ']';
        return out.str();
    }
    }

    return "<unknown>";
}

std::ostream& operator<<(std::ostream& out, const Value& value)
{
    out << valueToString(value);
    return out;
}
