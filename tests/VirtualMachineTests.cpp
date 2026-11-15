#include "VirtualMachine.hpp"
#include "TestSupport.hpp"

#include <string_view>

namespace {

constexpr auto byte = OperandWidth::Byte;
constexpr auto word = OperandWidth::Word;
constexpr auto qword = OperandWidth::QWord;
constexpr RegisterView rax{RegisterName::RAX, qword, 0};
constexpr RegisterView rbx{RegisterName::RBX, qword, 0};
constexpr RegisterView rcx{RegisterName::RCX, qword, 0};
constexpr RegisterView ax{RegisterName::RAX, word, 0};
constexpr RegisterView al{RegisterName::RAX, byte, 0};
constexpr RegisterView ah{RegisterName::RAX, byte, 8};

// A branch skips this malformed instruction only when its condition is correct.
// This lets tests check results through step() without accessing private state.
const Instruction failure{Opcode::MOV, qword, std::nullopt, std::nullopt};

void step(vm::VirtualMachine& machine, std::size_t count) {
    for (std::size_t i = 0; i < count; ++i)
        machine.step();
}

void expectEndOfProgram(vm::VirtualMachine& machine) {
    try {
        machine.step();
    } catch (const std::runtime_error& error) {
        test::expect(std::string_view(error.what()) == "instruction doesn't exist",
                     "VM must have reached the end of the program");
        return;
    }
    test::expect(false, "VM did not reach the expected end of the program");
}

void haltPreventsFurtherExecution() {
    // GIVEN a halt instruction followed by an invalid instruction
    vm::VirtualMachine machine({
        {Opcode::HLT, qword, std::nullopt, std::nullopt},
        failure
    });

    // WHEN the VM is stepped past the halt instruction
    machine.step();

    // THEN further steps do nothing instead of executing the invalid instruction
    step(machine, 3);
}

void emptyProgramThrowsAndHalts() {
    // GIVEN a VM with no instructions
    vm::VirtualMachine machine({});

    // WHEN it tries to fetch an instruction
    // THEN it reports the missing instruction and subsequent steps do nothing
    expectEndOfProgram(machine);
    machine.step();
}

void executesArithmeticAndComparison() {
    // GIVEN a program that should calculate RAX = ((10 + 20) - 5) + 1 - 1
    vm::VirtualMachine machine({
        {Opcode::MOV, qword, std::uint64_t{10}, rax},
        {Opcode::MOV, qword, std::uint64_t{20}, rbx},
        {Opcode::ADD, qword, rbx, rax},
        {Opcode::SUB, qword, std::uint64_t{5}, rax},
        {Opcode::INC, qword, rax, std::nullopt},
        {Opcode::DEC, qword, rax, std::nullopt},
        {Opcode::CMP, qword, std::uint64_t{25}, rax},
        {Opcode::JE, qword, std::uint64_t{9}, std::nullopt},
        failure
    });

    // WHEN the arithmetic, comparison, and conditional jump execute
    step(machine, 8);

    // THEN RAX equals 25, so the jump skips the failure instruction
    expectEndOfProgram(machine);
}

void movesBytesBetweenMemoryAndRegisterViews() {
    // GIVEN a program that loads AH from memory and writes AL separately
    vm::VirtualMachine machine({
        {Opcode::MOV, byte, std::uint64_t{0xAB}, MemoryAddress{16}},
        {Opcode::MOV, byte, MemoryAddress{16}, ah},
        {Opcode::MOV, byte, std::uint64_t{0xCD}, al},
        {Opcode::CMP, word, std::uint64_t{0xABCD}, ax},
        {Opcode::JE, qword, std::uint64_t{6}, std::nullopt},
        failure
    });

    // WHEN the moves and comparison execute
    step(machine, 5);

    // THEN AX contains 0xABCD and execution skips the failure instruction
    expectEndOfProgram(machine);
}

void jumpsToImmediateAndRegisterTargets() {
    // GIVEN a program with immediate and register jumps over invalid instructions
    vm::VirtualMachine machine({
        {Opcode::JMP, qword, std::uint64_t{2}, std::nullopt},
        failure,
        {Opcode::MOV, qword, std::uint64_t{5}, rax},
        {Opcode::JMP, qword, rax, std::nullopt},
        failure
    });

    // WHEN both jumps and the intervening move execute
    step(machine, 3);

    // THEN execution reaches instruction index five without hitting either failure
    expectEndOfProgram(machine);
}

void conditionalLoopStopsAtZero() {
    // GIVEN a loop that decrements RCX from three and branches while nonzero
    vm::VirtualMachine machine({
        {Opcode::MOV, qword, std::uint64_t{3}, rcx},
        {Opcode::DEC, qword, rcx, std::nullopt},
        {Opcode::JNZ, qword, std::uint64_t{1}, std::nullopt},
        {Opcode::CMP, qword, std::uint64_t{0}, rcx},
        {Opcode::JE, qword, std::uint64_t{6}, std::nullopt},
        failure
    });

    // WHEN three loop iterations and the final comparison execute
    step(machine, 9);

    // THEN RCX equals zero and execution leaves the loop at the expected step
    expectEndOfProgram(machine);
}

void rejectsMismatchedOperandWidths() {
    // GIVEN a 64-bit move targeting an 8-bit register view
    vm::VirtualMachine machine({
        {Opcode::MOV, qword, std::uint64_t{1}, al}
    });

    // WHEN the invalid instruction executes
    // THEN the VM rejects it and halts
    test::expectThrows<std::runtime_error>([&] { machine.step(); },
                                   "Mismatched register and instruction widths must be rejected");
    machine.step();
}

} // namespace

int main() {
    haltPreventsFurtherExecution();
    emptyProgramThrowsAndHalts();
    executesArithmeticAndComparison();
    movesBytesBetweenMemoryAndRegisterViews();
    jumpsToImmediateAndRegisterTargets();
    conditionalLoopStopsAtZero();
    rejectsMismatchedOperandWidths();
}
