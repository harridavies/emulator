#pragma once

#include <bit>
#include <cstring>
#include <cstdint>
#include <cstddef>
#include <new>

constexpr std::size_t DRAM_SIZE = 1000;
class MainMemory {
public:
    // allocate memory without constructing any objects.
    MainMemory() : buffer_(static_cast<std::byte*>(::operator new(sizeof(std::byte) * DRAM_SIZE))) {
        std::memset(buffer_, 0, (sizeof(std::byte) * DRAM_SIZE));
    }

    ~MainMemory() {
        // release memory.
        ::operator delete(buffer_);
    }

    // don't allow for copying.
    MainMemory(const MainMemory&) = delete;
    MainMemory& operator=(const MainMemory&) = delete;

    // move constructor.
    MainMemory(MainMemory&& other) noexcept {
        buffer_ = other.buffer_;
        other.buffer_ = nullptr;
    }
    // move assignment.
    MainMemory& operator=(MainMemory&& other) noexcept {
        if (this == &other) {
            return *this;
        }
        ::operator delete(buffer_);

        buffer_ = other.buffer_;
        other.buffer_ = nullptr;
        
        return *this;
    }

    std::uint8_t read8(const std::uint64_t address) const {
        validateAddress8(address);
        return static_cast<std::uint8_t>(buffer_[address]);
    }

    std::uint16_t read16(const std::uint64_t address) const {
        validateAddress16(address);
        std::uint16_t value{};
        std::memcpy(&value, &buffer_[address], (sizeof(std::byte) * 2));

        return value;
    }

    std::uint32_t read32(const std::uint64_t address) const {
        validateAddress32(address);
        std::uint32_t value{};
        std::memcpy(&value, &buffer_[address], (sizeof(std::byte) * 4));

        return value;
    }

    std::uint64_t read64(const std::uint64_t address) const {
        validateAddress64(address);
        std::uint64_t value{};
        std::memcpy(&value, &buffer_[address], (sizeof(std::byte) * 8));

        return value;
    }

    void write8(const std::uint64_t address, uint8_t value) {
        validateAddress8(address);
        buffer_[address] = static_cast<std::byte>(value);
    }

    void write16(const std::uint64_t address, uint16_t value) {
        validateAddress16(address);
        std::memcpy(&buffer_[address], &value, (sizeof(std::byte) * 2));
    }

    void write32(const std::uint64_t address, uint32_t value) {
        validateAddress32(address);
        std::memcpy(&buffer_[address], &value, (sizeof(std::byte) * 4));
    }

    void write64(const std::uint64_t address, uint64_t value) {
        validateAddress64(address);
        std::memcpy(&buffer_[address], &value, (sizeof(std::byte) * 8));
    }
private:
    std::byte* buffer_;

    static_assert(std::endian::native == std::endian::little);

    void validateAddress8(std::uint64_t address) const {
        if (buffer_ == nullptr) {
            throw "cannot access empty memory";
        }
        if (address >= DRAM_SIZE) {
            throw "address outside of DRAM memory (8 bit)";
        }
    }

    void validateAddress16(std::uint64_t address) const {
        if (buffer_ == nullptr) {
            throw "cannot access empty memory";
        }
        if (address > DRAM_SIZE - sizeof(std::uint16_t)) {
            throw "address outside of DRAM memory (16 bit)";
        }
    }

    void validateAddress32(std::uint64_t address) const {
        if (buffer_ == nullptr) {
            throw "cannot access empty memory";
        }
        if (address > DRAM_SIZE - sizeof(std::uint32_t)) {
            throw "address outside of DRAM memory (32 bit)";
        }
    }

    void validateAddress64(std::uint64_t address) const {
        if (buffer_ == nullptr) {
            throw "cannot access empty memory";
        }
        if (address > DRAM_SIZE - sizeof(std::uint64_t)) {
            throw "address outside of DRAM memory (64 bit)";
        }
    }
};