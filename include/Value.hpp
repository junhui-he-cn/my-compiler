#pragma once

#include <cstddef>
#include <ostream>
#include <string>

struct FunctionValue {
    std::string name;
    std::size_t functionIndex = 0;
    std::size_t arity = 0;
};

class Value {
public:
    enum class Type {
        Nil,
        Number,
        Bool,
        String,
        Function,
    };

    static Value nil();
    static Value number(double value);
    static Value boolean(bool value);
    static Value string(std::string value);
    static Value function(FunctionValue value);

    Type type() const;
    double asNumber() const;
    bool asBool() const;
    const std::string& asString() const;
    const FunctionValue& asFunction() const;

private:
    explicit Value(Type type);

    Type type_ = Type::Nil;
    double number_ = 0.0;
    bool boolean_ = false;
    std::string string_;
    FunctionValue function_;
};

bool isTruthy(const Value& value);
bool valuesEqual(const Value& left, const Value& right);
std::string valueToString(const Value& value);
std::ostream& operator<<(std::ostream& out, const Value& value);
