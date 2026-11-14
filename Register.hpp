#pragma once

#include <cstring>
#include <cstddef>
#include <cstdint>

class Register {
public:
    void write8(std::uint8_t value) noexcept {
        value_ &= ~std::uint64_t{0xFF};
        value_ |= value;
    }

    void writeHigh8(std::uint8_t value) noexcept {
        value_ &= ~std::uint64_t{0xFF00};
        value_ |= static_cast<std::uint64_t>(value) << 8;
    }

    void write16(std::uint16_t value) noexcept {
        value_ &= ~std::uint64_t{0xFFFF};
        value_ |= value;
    }

    void write32(std::uint32_t value) noexcept {
        // special case: need to zero upper 32 bits.
        value_ = value;
    }

    void write64(std::uint64_t value) noexcept {
        value_ = value;
    }

    [[nodiscard]] uint8_t read8() const noexcept {
        // don't need the cast but good to be explicit!
        return static_cast<std::uint8_t>(value_);
    }

    [[nodiscard]] uint8_t readHigh8() const noexcept {
        return static_cast<std::uint8_t>(value_ >> 8);
    }

    [[nodiscard]] uint16_t read16() const noexcept {
        return static_cast<std::uint16_t>(value_);
    }

    [[nodiscard]] uint32_t read32() const noexcept {
        return static_cast<std::uint32_t>(value_);
    }

    [[nodiscard]] uint64_t read64() const noexcept {
        return value_;
    }

private:
    std::uint64_t value_{};
};