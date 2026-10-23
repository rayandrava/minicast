#include "core.hpp"
#include <algorithm>
#include <cmath>
#include <cwctype>
#include <numbers>

namespace mini {
namespace {
// Bounded arithmetic, never a script engine. Unary minus binds below powers:
// -2^2 = -4, 2^-2 = .25. Postfix percent divides by 100.
class Calculator {
    std::wstring_view source;
    size_t position = 0;
    int depth = 0;
    bool valid = true;
    std::wstring error;
    void spaces() { while (position < source.size() && iswspace(source[position])) ++position; }
    bool take(wchar_t c) {
        spaces();
        if (position < source.size() && source[position] == c) { ++position; return true; }
        return false;
    }
    double checked(double value) {
        if (!std::isfinite(value) && error.empty()) error = L"Result is outside the supported range";
        return value;
    }
    double expression() {
        double value = product();
        while (valid) {
            if (take(L'+')) value = checked(value + product());
            else if (take(L'-')) value = checked(value - product());
            else break;
        }
        return value;
    }
    double product() {
        double value = unary();
        while (valid) {
            if (take(L'*') || take(L'×')) value = checked(value * unary());
            else if (take(L'/') || take(L'÷')) {
                double divisor = unary();
                if (divisor == 0) { error = L"Cannot divide by zero"; value = 0; }
                else value = checked(value / divisor);
            } else break;
        }
        return value;
    }
    double unary() {
        if (++depth > 32) { valid = false; --depth; return 0; }
        double value;
        if (take(L'+')) value = unary();
        else if (take(L'-')) value = -unary();
        else {
            value = primary();
            while (take(L'%')) value /= 100;
            if (take(L'^')) value = checked(std::pow(value, unary()));
        }
        --depth;
        return value;
    }
    double primary() {
        spaces();
        if (take(L'(')) { double value = expression(); if (!take(L')')) valid = false; return value; }
        size_t begin = position;
        while (position < source.size() && ((source[position] >= L'a' && source[position] <= L'z') || source[position] == L'π')) ++position;
        if (position != begin) {
            auto name = source.substr(begin, position - begin);
            if (name == L"pi" || name == L"π") return std::numbers::pi;
            if (name == L"e") return std::numbers::e;
            if (!take(L'(')) { valid = false; return 0; }
            double value = expression();
            if (!take(L')')) valid = false;
            if (name == L"sqrt") {
                if (value < 0) { error = L"Square root needs a nonnegative number"; return 0; }
                return std::sqrt(value);
            }
            if (name == L"abs") return std::abs(value);
            if (name == L"round") return std::round(value);
            if (name == L"floor") return std::floor(value);
            if (name == L"ceil") return std::ceil(value);
            if (name == L"sin") return checked(std::sin(value));
            if (name == L"cos") return checked(std::cos(value));
            if (name == L"tan") return checked(std::tan(value));
            if (name == L"ln" || name == L"log") {
                if (value <= 0) { error = L"Logarithm needs a positive number"; return 0; }
                return name == L"ln" ? std::log(value) : std::log10(value);
            }
            if (name == L"exp") return checked(std::exp(value));
            valid = false; return 0;
        }
        // Locale-independent decimals, including scientific notation.
        double value = 0, fraction = .1; bool digits = false;
        auto digit = [&] { return position < source.size() && source[position] >= L'0' && source[position] <= L'9'; };
        while (digit()) { digits = true; value = value * 10 + source[position++] - L'0'; }
        if (position < source.size() && source[position] == L'.') {
            ++position;
            while (digit()) { digits = true; value += (source[position++] - L'0') * fraction; fraction *= .1; }
        }
        if (!digits) { valid = false; return 0; }
        if (position < source.size() && source[position] == L'e') {
            ++position; int sign = 1, exponent = 0;
            if (position < source.size() && (source[position] == L'+' || source[position] == L'-')) sign = source[position++] == L'-' ? -1 : 1;
            if (!digit()) valid = false;
            while (digit()) exponent = std::min(10000, exponent * 10 + int(source[position++] - L'0'));
            value *= std::pow(10., sign * exponent);
        }
        return checked(value);
    }
public:
    explicit Calculator(std::wstring_view input) : source(input) {}
    std::optional<Calculation> run() {
        double value = expression(); spaces();
        if (!valid || position != source.size()) return {};
        if (!error.empty()) return Calculation{error, true};
        return Calculation{format_number(value, 12), false};
    }
};
}
std::optional<Calculation> calculate(const std::wstring& input) {
    if (input.empty() || input.size() > 256) return {};
    auto source = lower(trim(input));
    bool explicitCalculation = source.starts_with(L'=');
    if (explicitCalculation) source.erase(0, 1);
    // Plain numbers remain app queries; expressions and constants opt in.
    if (!explicitCalculation && source.find_first_of(L"+-*/×÷^%()abcdefghijklmnopqrstuvwxyzπ") == source.npos) return {};
    auto of = source.find(L"% of ");
    if (of != source.npos) source.replace(of, 5, L"% * ");
    return Calculator(source).run();
}
}
