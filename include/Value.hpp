#pragma once

#include <ostream>
#include <string>

class Value {
public:
    enum class Type {
        Nil,
        Number,
        Bool,
        String,
    };

    static Value nil();
    static Value number(double value);
    static Value boolean(bool value);
    static Value string(std::string value);

    Type type() const;
    double asNumber() const;
    bool asBool() const;
    const std::string& asString() const;

private:
    explicit Value(Type type);

    Type type_ = Type::Nil;
    double number_ = 0.0;
    bool boolean_ = false;
    std::string string_;
};

bool isTruthy(const Value& value);
bool valuesEqual(const Value& left, const Value& right);
std::string valueToString(const Value& value);
std::ostream& operator<<(std::ostream& out, const Value& value);
