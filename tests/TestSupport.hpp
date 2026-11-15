#pragma once

#include <stdexcept>
#include <iostream>
#include <sstream>
#include <string>

namespace test {

// Unlike assert(), these checks remain active in Release builds.
inline void expect(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}

template <typename Exception, typename Action>
void expectThrows(Action action, const char* message) {
    try {
        action();
    } catch (const Exception&) {
        return;
    }
    throw std::runtime_error(message);
}

template <typename Action>
std::string captureOutput(Action action) {
    std::ostringstream output;
    auto* original = std::cout.rdbuf(output.rdbuf());
    try {
        action();
    } catch (...) {
        std::cout.rdbuf(original);
        throw;
    }
    std::cout.rdbuf(original);
    return output.str();
}

} // namespace test
