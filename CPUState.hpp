#pragma once

#include "Instruction.hpp"
#include "Register.hpp"
#include "MainMemory.hpp"

#include <array>
#include <iostream>

enum class State {
    HALTED,
    RUNNING
};

struct CPUState {
    std::array<Register, numRegisters> registers_{};
    MainMemory memory_;
    std::size_t ip_{}; // instruction pointer
    State state_{State::RUNNING};
    uint64_t carryFlag_{};
    uint64_t zeroFlag_{};
    uint64_t signFlag_{};
    uint64_t overflowFlag_{};

    std::size_t getRegisterIndex(RegisterName registerName) const {
        switch (registerName) {
            case (RegisterName::RAX):
                return 0;
            case (RegisterName::RBX):
                return 1;
            case (RegisterName::RCX):
                return 2;
            case (RegisterName::RDX):
                return 3;
            default:
                throw "register doesn't exist";
        } // end switch
    }

    void debugRegisters() const {
        constexpr std::array<std::string_view, numRegisters> names{
            "RAX", "RBX", "RCX", "RDX"
        };

        for (std::size_t i{}; i < registers_.size(); ++i) {
            std::cout << names[i] << registers_[i].read64() << '\n';
        }
    }
};
