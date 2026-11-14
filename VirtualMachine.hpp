#pragma once

#include "Instruction.hpp"
#include "CPUState.hpp"

#include <cstdint>
#include <utility>
#include <variant>
#include <vector>

namespace vm {

class VirtualMachine {
public:
    VirtualMachine(std::vector<Instruction> instructions) : instructions_(std::move(instructions)) {}
 
    void step() {
        if (state_.state_ == State::HALTED) {
            return;
        }
        
        std::size_t ip = state_.ip_;

        if (ip >= instructions_.size()) {
            state_.state_ = State::HALTED;
            throw "instruction doesn't exist";
        }
        
        const Instruction& instruction = instructions_[ip];

        switch (instruction.opcode_) {
            case Opcode::MOV : {
                // movq %rax, %rbx
                executeMov(instruction);
                break;
            }
            case Opcode::ADD : {
                // addq $5x, %rax
                executeAdd(instruction);
                break;
            }
            case Opcode::SUB : {
                // subq $5, %rax
                executeSub(instruction);
                break; 
            }
            case Opcode::INC : {
                // incq %rax
                executeInc(instruction);
                break;   
            }
            case Opcode::DEC : {
                // incq %rax
                executeDec(instruction);
                break;   
            }
            case Opcode::JMP : {
                // jmp $10
                // jmp %rax also fine
                executeJmp(instruction);
                break;
            }
            case Opcode::CMP : {
                executeCmp(instruction); 
                break;
            }
            case Opcode::AND : {
                executeAnd(instruction);
                break;
            }
            case Opcode::JE : 
            case Opcode::JZ : {
                executeConditionalJump(instruction, state_.zeroFlag_);
                break;
            }
            case Opcode::JNE :
            case Opcode::JNZ : {
                executeConditionalJump(instruction, !state_.zeroFlag_);
                break;
            }
            case Opcode::JC : {
                executeConditionalJump(instruction, state_.carryFlag_);
                break;
            }
            case Opcode::JNC : {
                executeConditionalJump(instruction, !state_.carryFlag_);
                break;
            }
            case Opcode::JG : {
                executeConditionalJump(instruction, !state_.zeroFlag_ && state_.signFlag_ == state_.overflowFlag_);
                break;
            }
            case Opcode::JL : {
                executeConditionalJump(instruction, state_.signFlag_ != state_.overflowFlag_);
                break;
            }
            case Opcode::HLT : {
                state_.state_ = State::HALTED;
                return;
            }
            default : {
                state_.state_ = State::HALTED;
                throw "unsupported opcode";
            }
        } // end switch
    }

private:
    std::vector<Instruction> instructions_;
    CPUState state_;

    std::uint64_t readOperand(const std::optional<Operand>& operand, OperandWidth instructionWidth) {
        if (!operand.has_value()) {
            state_.state_ = State::HALTED;
            throw "src operand empty";
        }

        if (std::holds_alternative<RegisterView>(operand.value())) {
            const auto& view = std::get<RegisterView>(operand.value());
            
            validateRegisterView(view);

            std::size_t registerIndex = state_.getRegisterIndex(view.register_);

            switch (view.width_) {
                case OperandWidth::Byte : {
                    if (view.bitOffset_ == 8) {
                        return static_cast<std::uint64_t>(state_.registers_[registerIndex].readHigh8());
                    } else if (view.bitOffset_ == 0) {
                        return static_cast<std::uint64_t>(state_.registers_[registerIndex].read8());
                    } else {
                        state_.state_ = State::HALTED; 
                        throw "operand offset invalid";
                        break;
                    }
                }
                case OperandWidth::Word : {
                    return static_cast<std::uint64_t>(state_.registers_[registerIndex].read16());
                }
                case OperandWidth::DWord : {
                    return static_cast<std::uint64_t>(state_.registers_[registerIndex].read32());
                }
                case OperandWidth::QWord : {
                    return static_cast<std::uint64_t>(state_.registers_[registerIndex].read64());
                }
                default : {
                    state_.state_ = State::HALTED; 
                    throw "operand width does not exist";
                }
            }
        } else if (std::holds_alternative<MemoryAddress>(operand.value())) {
            switch (instructionWidth) {
                case OperandWidth::Byte : {
                    return state_.memory_.read8((std::get<MemoryAddress>(operand.value())).address_);
                }
                case OperandWidth::Word : {
                    return state_.memory_.read16((std::get<MemoryAddress>(operand.value())).address_);
                }
                case OperandWidth::DWord : {
                    return state_.memory_.read32((std::get<MemoryAddress>(operand.value())).address_);
                }
                case OperandWidth::QWord : {
                    return state_.memory_.read64((std::get<MemoryAddress>(operand.value())).address_);
                }
                default : {
                    state_.state_ = State::HALTED;
                    throw "invalid memory operand width";
                }
            }
        } else if (std::holds_alternative<std::uint64_t>(operand.value())) {
            return std::get<std::uint64_t>(operand.value());
        } else {
            state_.state_ = State::HALTED;
            throw "src operand must be an immediate, register, or memory address"; 
        }
    }

    void writeOperand(const std::optional<Operand>& operand, std::uint64_t value, OperandWidth instructionWidth) {
        if (!operand.has_value()) {
            state_.state_ = State::HALTED;
            throw "dest operand empty"; 
        }

        if (std::holds_alternative<RegisterView>(operand.value())) {
            const auto& view = std::get<RegisterView>(operand.value());
            
            validateRegisterView(view);

            std::size_t registerIndex = state_.getRegisterIndex(view.register_);

            switch (view.width_) {
                case (OperandWidth::Byte) : {
                    if (view.bitOffset_ == 8) {
                        state_.registers_[registerIndex].writeHigh8(value);
                    } else if (view.bitOffset_ == 0) {
                        state_.registers_[registerIndex].write8(value);
                    } else {
                        state_.state_ = State::HALTED; 
                        throw "operand offset invalid"; 
                    }
                    break;
                }
                case (OperandWidth::Word) : {
                    state_.registers_[registerIndex].write16(value);
                    break;
                }
                case (OperandWidth::DWord) : {
                    state_.registers_[registerIndex].write32(value);
                    break;
                }
                case (OperandWidth::QWord) : {
                    state_.registers_[registerIndex].write64(value);
                    break;
                }
                default : {
                    state_.state_ = State::HALTED; 
                    throw "operand width does not exist";
                } 
            }
        } else if (std::holds_alternative<MemoryAddress>(operand.value())) {
            switch (instructionWidth) {
                case OperandWidth::Byte : {
                    state_.memory_.write8((std::get<MemoryAddress>(operand.value())).address_, value);
                    return;
                }
                case OperandWidth::Word : {
                    state_.memory_.write16((std::get<MemoryAddress>(operand.value())).address_, value); 
                    return;
                }
                case OperandWidth::DWord : {
                    state_.memory_.write32((std::get<MemoryAddress>(operand.value())).address_, value);
                    return;
                }
                case OperandWidth::QWord : {
                    state_.memory_.write64((std::get<MemoryAddress>(operand.value())).address_, value);
                    return;
                }
                default : {
                    state_.state_ = State::HALTED;
                    throw "invalid memory operand width";
                }
            }
        } else {
            state_.state_ = State::HALTED;
            throw "dest operand must be a register, or memory address"; 
        }
    }

    void checkOneOperands(const Instruction& instruction, bool skipWidth = false) {
        if (!instruction.first_.has_value()) {
            state_.state_ = State::HALTED;
            throw "this operation must have only one operand";
        }
        if (instruction.second_.has_value()) {
            state_.state_ = State::HALTED;
            throw "this operation must have only one operand";
        }
        if (skipWidth) {
            return;
        }
        validateOperandWidth(instruction.first_.value(), instruction.width_);
    }

    void checkTwoOperands(const Instruction& instruction, bool skipWidth = false) {
        if (!instruction.first_.has_value() || !instruction.second_.has_value()) {
            state_.state_ = State::HALTED;
            throw "this operation must have two operands";
        }
        if (skipWidth) {
            return;
        }
        validateOperandWidth(instruction.first_.value(), instruction.width_);
        validateOperandWidth(instruction.second_.value(), instruction.width_);
    }

    void validateRegisterView(const RegisterView& view) {
        // normal register views always begin at bit 0.
        if (view.bitOffset_ == 0) {
            switch (view.width_) {
                case OperandWidth::Byte:
                case OperandWidth::Word:
                case OperandWidth::DWord:
                case OperandWidth::QWord:
                    return;
                default:
                    break;
            }
        }

        // high-byte views exist only for AH, BH, CH and DH.
        if (view.bitOffset_ == 8 &&
            view.width_ == OperandWidth::Byte) {

            switch (view.register_) {
                case RegisterName::RAX:
                case RegisterName::RBX:
                case RegisterName::RCX:
                case RegisterName::RDX:
                    return;
                default:
                    break;
            }
        }

        state_.state_ = State::HALTED;
        throw "invalid register view";
    }

    void validateOperandWidth(const Operand& operand, OperandWidth instructionWidth) {
        if (!std::holds_alternative<RegisterView>(operand)) {
            return;
        }
        if (std::get<RegisterView>(operand).width_ != instructionWidth) {
            state_.state_ = State::HALTED;
            throw "regsiter and instruction widths do not match";
        }
    }

    void updateSubFlags(std::uint64_t src, std::uint64_t dest, std::uint64_t res, OperandWidth width) {
        std::uint64_t mask = maskForWidth(width);
        const std::uint64_t signbit = static_cast<uint64_t>(width) - 1;

        src &= mask;
        dest &= mask;
        res &= mask;

        state_.zeroFlag_ = (res == 0);

        // unsigned borrow occurs when the destination is smaller than the source
        state_.carryFlag_ = (dest < src);
        
        // sign bit is 63 (for 8 bytes), so just check that
        state_.signFlag_ = ((res >> signbit) & 1) != 0;
        
        // signed subtraction overflows when the operands have different signs and the result
        // has a different sign from the destination
        state_.overflowFlag_ = (((dest ^ src) & (dest ^ res)) >> signbit) & 1;
    }

    void updateAddFlags(std::uint64_t src, std::uint64_t dest, std::uint64_t res, OperandWidth width) {
        std::uint64_t mask = maskForWidth(width);
        const std::uint64_t signbit = static_cast<uint64_t>(width) - 1;

        src &= mask;
        dest &= mask;
        res &= mask;

        state_.zeroFlag_ = (res == 0);
        state_.signFlag_ = ((res >> signbit) & 1) != 0;
        state_.carryFlag_ = (res < dest);
        state_.overflowFlag_ = (((~(dest ^ src)) & (dest ^ res)) >> signbit) & 1;
    }

    std::uint64_t maskForWidth(OperandWidth width) {
        switch (width) {
            case OperandWidth::Byte:  return 0xFFULL;
            case OperandWidth::Word:  return 0xFFFFULL;
            case OperandWidth::DWord: return 0xFFFFFFFFULL;
            case OperandWidth::QWord: return 0xFFFFFFFFFFFFFFFFULL;
            default: throw "invalid operand width";
        }
    }

    void executeMov(const Instruction& instruction) {
        checkTwoOperands(instruction);
        
        std::uint64_t value = readOperand(instruction.first_);
        writeOperand(instruction.second_, value);

        state_.ip_++;
    }
    
    void executeAdd(const Instruction& instruction) {
        checkTwoOperands(instruction);

        std::uint64_t lhs = readOperand(instruction.first_);
        std::uint64_t rhs = readOperand(instruction.second_);
        
        writeOperand(instruction.second_, lhs + rhs);
        updateAddFlags(lhs, rhs, rhs + lhs, instruction.width_);

        state_.ip_++;
    }

    void executeSub(const Instruction& instruction) {
        checkTwoOperands(instruction);

        std::uint64_t lhs = readOperand(instruction.first_);
        std::uint64_t rhs = readOperand(instruction.second_);

        writeOperand(instruction.second_, rhs - lhs);
        updateSubFlags(lhs, rhs, rhs - lhs, instruction.width_);

        state_.ip_++;
    }

    void executeInc(const Instruction& instruction) {
        checkOneOperands(instruction);

        std::uint64_t value = readOperand(instruction.first_);
        writeOperand(instruction.first_, value + 1);

        bool cf = state_.carryFlag_;
        updateAddFlags(1, value, value + 1, instruction.width_);
        state_.carryFlag_ = cf;

        state_.ip_++;
    }

    void executeDec(const Instruction& instruction) {
        checkOneOperands(instruction);

        std::uint64_t value = readOperand(instruction.first_);
        writeOperand(instruction.first_, value - 1); 

        bool cf = state_.carryFlag_;
        updateSubFlags(1, value, value - 1, instruction.width_);
        state_.carryFlag_ = cf;
        
        state_.ip_++;
    }

    void executeJmp(const Instruction& instruction) {
        checkOneOperands(instruction, true);
        state_.ip_ = readOperand(instruction.first_);
    }

    void executeCmp(const Instruction& instruction) {
        checkTwoOperands(instruction);

        std::uint64_t lhs = readOperand(instruction.first_);
        std::uint64_t rhs = readOperand(instruction.second_);

        updateSubFlags(lhs, rhs, rhs - lhs, instruction.width_);
        state_.ip_++;
    }

    void executeAnd(const Instruction& instruction) {
        checkTwoOperands(instruction);

        std::uint64_t lhs = readOperand(instruction.first_);
        std::uint64_t rhs = readOperand(instruction.second_);
        std::uint64_t res = rhs & lhs;

        std::uint64_t mask = maskForWidth(instruction.width_);
        std::uint64_t signbit = static_cast<uint64_t>(instruction.width_) - 1;

        res &= mask;

        writeOperand(instruction.second_, res);

        state_.zeroFlag_ = (res == 0) ? 1 : 0;
        state_.signFlag_ = ((res >> signbit) & 1) != 0;
        state_.carryFlag_ = 0;
        state_.overflowFlag_ = 0;

        state_.ip_++;
    }

    void executeConditionalJump(const Instruction& instruction, bool condition) {
        if (condition) {
            executeJmp(instruction);
        } else {
            state_.ip_++;
        }
    }
};

} // namespace vm