#pragma once

#include "VirtualMachine.hpp"
#include "Instruction.hpp"
#include "CPUState.hpp"

#include <unordered_set>

namespace debugger {

class Debugger {
public:
    Debugger(std::vector<Instruction> instructions) : vm_(instructions) {}

    void step() {
        vm_.step();
    }

    void continueExecution() {
        while (vm_.state().state_ != State::HALTED && !breakpoint_.contains(vm_.state().ip_)){
            step();
        }
    }

    void setBreakpoint(std::size_t ip) {
        breakpoint_.insert(ip);
    }

    void removeBreakPoint(std::size_t ip) {
        breakpoint_.erase(ip);
    }

    void inspectState() const {
        vm_.state().debugRegisters();
        vm_.state().debugFlags();
        vm_.state().debugIp();
    }

    void currentInstruction() const {
        vm_.debugInstruction();
    }

    void status() const {
        vm_.state().status();
    }

private:
    vm::VirtualMachine vm_;
    std::unordered_set<std::size_t> breakpoint_;
};

} // namespace debugger