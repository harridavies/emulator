#pragma once

#include <cstdint>
#include <optional>
#include <variant>

enum class Opcode {
    MOV,
    ADD,
    SUB,
    INC,
    DEC,
    JMP,
    CMP,
    AND,
    OR,
    XOR,
    NOT,
    JE,
    JZ,
    JNE,
    JNZ,
    JC,
    JNC,
    JG,
    JL,
    HLT
};

constexpr std::size_t numRegisters = 4;

enum class RegisterName {
    RAX,
    RBX,
    RCX,
    RDX
};

enum class OperandWidth {
    Byte = 8,
    Word = 16,
    DWord = 32,
    QWord = 64
};

struct RegisterView {
    RegisterName register_;
    OperandWidth width_;
    std::uint64_t bitOffset_;
};


struct MemoryAddress {
    std::uint64_t address_;
};

using Operand = std::variant<RegisterView, std::uint64_t, MemoryAddress>;

struct Instruction {
    Opcode opcode_;
    OperandWidth width_;
    std::optional<Operand> first_;
    std::optional<Operand> second_;
};