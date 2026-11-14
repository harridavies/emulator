#pragma once

#include <stdexcept>

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

} // namespace test
