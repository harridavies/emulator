#pragma once

#include <cstdint>
#include <optional>
#include <variant>
#include <iostream>
#include <vector>

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

    static constexpr std::string_view opcodeName(Opcode opcode) {
        constexpr std::array names{
            "MOV", "ADD", "SUB", "INC", "DEC", "JMP",
            "CMP", "AND", "OR", "XOR", "NOT", "JE", "JZ",
            "JNE", "JNZ", "JC", "JNC", "JG", "JL", "HLT"
        };

        return names.at(static_cast<std::size_t>(opcode));
    }

    static constexpr std::string_view registerName(RegisterName reg) {
        constexpr std::array names{"rax", "rbx", "rcx", "rdx"};
        return names.at(static_cast<std::size_t>(reg));
    }

    static constexpr char widthSuffix(OperandWidth width) {
        switch (width) {
            case OperandWidth::Byte:  return 'b';
            case OperandWidth::Word:  return 'w';
            case OperandWidth::DWord: return 'l';
            case OperandWidth::QWord: return 'q';
        }

        return '?';
    }

    static void debugOperand(const Operand& operand) {
        std::visit([](const auto& value) {
            using T = std::decay_t<decltype(value)>;

            if constexpr (std::is_same_v<T, RegisterView>) {
                std::cout << '%';

                const auto reg = value.register_;
                const auto width = value.width_;

                if (width == OperandWidth::Byte) {
                    const bool high = value.bitOffset_ == 8;

                    switch (reg) {
                        case RegisterName::RAX: std::cout << (high ? "ah" : "al"); break;
                        case RegisterName::RBX: std::cout << (high ? "bh" : "bl"); break;
                        case RegisterName::RCX: std::cout << (high ? "ch" : "cl"); break;
                        case RegisterName::RDX: std::cout << (high ? "dh" : "dl"); break;
                    }
                } else if (width == OperandWidth::Word) {
                    switch (reg) {
                        case RegisterName::RAX: std::cout << "ax"; break;
                        case RegisterName::RBX: std::cout << "bx"; break;
                        case RegisterName::RCX: std::cout << "cx"; break;
                        case RegisterName::RDX: std::cout << "dx"; break;
                    }
                } else if (width == OperandWidth::DWord) {
                    switch (reg) {
                        case RegisterName::RAX: std::cout << "eax"; break;
                        case RegisterName::RBX: std::cout << "ebx"; break;
                        case RegisterName::RCX: std::cout << "ecx"; break;
                        case RegisterName::RDX: std::cout << "edx"; break;
                    }
                } else {
                    std::cout << registerName(reg);
                }

            } else if constexpr (std::is_same_v<T, std::uint64_t>) {
                std::cout << '$' << value;

            } else if constexpr (std::is_same_v<T, MemoryAddress>) {
                std::cout << "0x" << std::hex << value.address_
                        << std::dec;
            }
        }, operand);
    }

    void debugInstruction() const {
        std::cout << opcodeName(opcode_);

        if (opcode_ != Opcode::HLT) {
            std::cout << widthSuffix(width_);
        }

        if (first_) {
            std::cout << ' ';
            debugOperand(*first_);
        }

        if (second_) {
            std::cout << ", ";
            debugOperand(*second_);
        }

        std::cout << '\n';
    }
};