#include "VirtualMachine.hpp"
#include "TestSupport.hpp"

#include <string_view>
#include <utility>
#include <vector>

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

void runHaltsBeforeInstructionLimit() {
    // GIVEN a short program that writes a register and halts
    vm::VirtualMachine machine({
        {Opcode::MOV, qword, std::uint64_t{42}, rax},
        {Opcode::HLT, qword, std::nullopt, std::nullopt}
    });

    // WHEN the program runs within its instruction budget
    machine.run();

    // THEN it completes normally with the expected result
    test::expect(machine.state().state_ == State::HALTED, "Program must halt normally");
    test::expect(machine.state().registers_[0].read64() == 42, "Program must finish its work");
}

void runAllowsHaltOnFinalPermittedInstruction() {
    // GIVEN 999 increments followed by HLT as instruction number 1000
    std::vector<Instruction> instructions(999, {Opcode::INC, qword, rax, std::nullopt});
    instructions.push_back({Opcode::HLT, qword, std::nullopt, std::nullopt});
    vm::VirtualMachine machine(std::move(instructions));

    // WHEN the program uses its entire instruction budget
    machine.run();

    // THEN the final HLT succeeds without reporting a limit error
    test::expect(machine.state().state_ == State::HALTED, "Final permitted HLT must succeed");
    test::expect(machine.state().registers_[0].read64() == 999, "All increments must execute");
    test::expect(machine.state().ip_ == 999, "Execution must stop at HLT");
}

void runStopsAtInstructionLimit() {
    // GIVEN an infinite loop that increments RAX once every two instructions
    vm::VirtualMachine machine({
        {Opcode::INC, qword, rax, std::nullopt},
        {Opcode::JMP, qword, std::uint64_t{0}, std::nullopt}
    });

    // WHEN execution exhausts its 1000-instruction budget
    // THEN it reports an error and halts before executing instruction number 1001
    test::expectThrows<std::runtime_error>([&] { machine.run(); },
                                          "Infinite loop must exhaust its instruction budget");
    test::expect(machine.state().state_ == State::HALTED, "Budget exhaustion must halt execution");
    test::expect(machine.state().registers_[0].read64() == 500, "Exactly 1000 instructions must execute");
    test::expect(machine.state().ip_ == 0, "Final jump must leave the instruction pointer at zero");
}

void inspectionRejectsMissingInstructions() {
    // GIVEN an empty program and a program that jumps outside its instruction list
    vm::VirtualMachine empty({});
    vm::VirtualMachine jumped({{Opcode::JMP, qword, std::uint64_t{99}, std::nullopt}});
    jumped.step();

    // WHEN their current instructions are inspected
    // THEN both report an error without accessing invalid memory or changing state
    for (auto* machine : {&empty, &jumped}) {
        try {
            machine->debugInstruction();
            test::expect(false, "Missing current instruction must be rejected");
        } catch (const std::runtime_error& error) {
            test::expect(std::string_view(error.what()) == "no valid current instruction",
                         "Inspection must explain that no valid instruction exists");
        }
        test::expect(machine->state().state_ == State::RUNNING, "Inspection must not change execution state");
    }
    test::expect(jumped.state().ip_ == 99, "Inspection must not change the instruction pointer");
}

void inspectionDisplaysValidInstructionsAfterHalting() {
    // GIVEN a program whose current instruction is HLT
    vm::VirtualMachine machine({{Opcode::HLT, qword, std::nullopt, std::nullopt}});

    // WHEN the instruction is inspected before and after execution
    const auto before = test::captureOutput([&] { machine.debugInstruction(); });
    machine.run();
    const auto after = test::captureOutput([&] { machine.debugInstruction(); });

    // THEN the valid instruction remains inspectable in both states
    test::expect(before == "HLT\n" && after == before, "Valid HLT must remain inspectable");
    test::expect(machine.state().state_ == State::HALTED, "Inspection must preserve the halted state");
}

constexpr std::pair<OperandWidth, std::uint64_t> logicalWidths[] = {
    {OperandWidth::Byte, 0xFFULL},
    {OperandWidth::Word, 0xFFFFULL},
    {OperandWidth::DWord, 0xFFFFFFFFULL},
    {OperandWidth::QWord, 0xFFFFFFFFFFFFFFFFULL}
};

void orUpdatesResultAndFlagsAtEachWidth() {
    // GIVEN each width and an addition that sets both carry and overflow
    for (const auto& [width, mask] : logicalWidths) {
        const RegisterView destination{RegisterName::RAX, width, 0};
        const auto signBit = mask ^ (mask >> 1);
        vm::VirtualMachine machine({
            {Opcode::MOV, width, signBit, destination},
            {Opcode::ADD, width, signBit, destination},
            {Opcode::MOV, width, std::uint64_t{5}, destination},
            {Opcode::OR, width, signBit, destination},
            {Opcode::HLT, qword, std::nullopt, std::nullopt}
        });

        // WHEN OR combines an immediate source with the register destination
        machine.run();

        // THEN the result sets sign, clears zero, and clears carry and overflow
        const auto& state = machine.state();
        test::expect(state.registers_[0].read64() == (signBit | 5), "Incorrect OR result");
        test::expect(state.signFlag_ == 1 && state.zeroFlag_ == 0, "Incorrect OR result flags");
        test::expect(state.carryFlag_ == 0 && state.overflowFlag_ == 0, "OR must clear carry and overflow");
    }
}

void xorUpdatesResultAndFlagsAtEachWidth() {
    // GIVEN equal register operands at each width and previously set carry/overflow
    for (const auto& [width, mask] : logicalWidths) {
        const RegisterView destination{RegisterName::RAX, width, 0};
        const RegisterView source{RegisterName::RBX, width, 0};
        const auto signBit = mask ^ (mask >> 1);
        vm::VirtualMachine machine({
            {Opcode::MOV, width, signBit, destination},
            {Opcode::ADD, width, signBit, destination},
            {Opcode::MOV, width, std::uint64_t{0xA5}, destination},
            {Opcode::MOV, width, std::uint64_t{0xA5}, source},
            {Opcode::XOR, width, source, destination},
            {Opcode::HLT, qword, std::nullopt, std::nullopt}
        });

        // WHEN XOR combines the equal operands
        machine.run();

        // THEN the result is zero, with sign/carry/overflow cleared and source preserved
        const auto& state = machine.state();
        test::expect(state.registers_[0].read64() == 0, "Equal XOR operands must produce zero");
        test::expect(state.registers_[1].read64() == 0xA5, "XOR must preserve its source");
        test::expect(state.zeroFlag_ == 1 && state.signFlag_ == 0, "Incorrect XOR result flags");
        test::expect(state.carryFlag_ == 0 && state.overflowFlag_ == 0, "XOR must clear carry and overflow");
    }
}

void notRespectsWidthAndPreservesFlags() {
    // GIVEN each width with carry, overflow, and zero set by an earlier addition
    for (const auto& [width, mask] : logicalWidths) {
        const RegisterView destination{RegisterName::RAX, width, 0};
        const auto signBit = mask ^ (mask >> 1);
        vm::VirtualMachine machine({
            {Opcode::MOV, width, signBit, destination},
            {Opcode::ADD, width, signBit, destination},
            {Opcode::MOV, width, std::uint64_t{1}, destination},
            {Opcode::NOT, width, destination, std::nullopt},
            {Opcode::HLT, qword, std::nullopt, std::nullopt}
        });

        // WHEN NOT inverts the operand
        machine.run();

        // THEN only bits within its width change and every flag is preserved
        const auto& state = machine.state();
        test::expect(state.registers_[0].read64() == mask - 1, "Incorrect width-limited NOT result");
        test::expect(state.carryFlag_ == 1 && state.overflowFlag_ == 1 &&
                     state.zeroFlag_ == 1 && state.signFlag_ == 0, "NOT must preserve all flags");
    }
}

void bitwiseInstructionsSupportMemoryDestinations() {
    // GIVEN a memory destination at each width with populated surrounding bytes
    for (const auto& [width, mask] : logicalWidths) {
        vm::VirtualMachine machine({
            {Opcode::MOV, qword, std::uint64_t{0xFFFFFFFFFFFFFFFFULL}, MemoryAddress{16}},
            {Opcode::MOV, width, std::uint64_t{0xA0}, MemoryAddress{16}},
            {Opcode::OR, width, std::uint64_t{0x0F}, MemoryAddress{16}},
            {Opcode::XOR, width, std::uint64_t{0xFF}, MemoryAddress{16}},
            {Opcode::NOT, width, MemoryAddress{16}, std::nullopt},
            {Opcode::HLT, qword, std::nullopt, std::nullopt}
        });

        // WHEN OR, XOR, and NOT modify the memory operand
        machine.run();

        // THEN the selected bytes contain NOT(0x50) and the other bytes are unchanged
        const auto expected = ~mask | (mask ^ 0x50ULL);
        test::expect(machine.state().memory_.read64(16) == expected, "Incorrect memory bitwise result or width");
    }
}

void bitwiseInstructionsPreserveOtherRegisterBits() {
    // GIVEN a full register containing distinct bytes
    vm::VirtualMachine machine({
        {Opcode::MOV, qword, std::uint64_t{0x1122334455667788ULL}, rax},
        {Opcode::OR, byte, std::uint64_t{0x80}, ah},
        {Opcode::XOR, byte, std::uint64_t{0xFF}, al},
        {Opcode::NOT, byte, ah, std::nullopt},
        {Opcode::HLT, qword, std::nullopt, std::nullopt}
    });

    // WHEN the high and low byte views are modified
    machine.run();

    // THEN the remaining register bits are preserved
    test::expect(machine.state().registers_[0].read64() == 0x1122334455660877ULL,
                 "Bitwise byte operations must preserve other register bits");
}

void bitwiseInstructionsValidateOperands() {
    // GIVEN malformed bitwise instructions with wrong arity, widths, or destinations
    const Instruction invalid[] = {
        {Opcode::OR, qword, std::nullopt, rax},
        {Opcode::XOR, qword, rax, std::nullopt},
        {Opcode::NOT, byte, al, al},
        {Opcode::OR, qword, std::uint64_t{1}, al},
        {Opcode::XOR, qword, std::uint64_t{1}, std::uint64_t{2}},
        {Opcode::NOT, qword, std::uint64_t{1}, std::nullopt}
    };

    // WHEN each malformed instruction executes
    // THEN it is rejected and the VM halts
    for (const auto& instruction : invalid) {
        vm::VirtualMachine machine({instruction});
        test::expectThrows<std::runtime_error>([&] { machine.step(); }, "Invalid bitwise operands accepted");
        test::expect(machine.state().state_ == State::HALTED, "Invalid operands must halt execution");
    }
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
    runHaltsBeforeInstructionLimit();
    runAllowsHaltOnFinalPermittedInstruction();
    runStopsAtInstructionLimit();
    inspectionRejectsMissingInstructions();
    inspectionDisplaysValidInstructionsAfterHalting();
    orUpdatesResultAndFlagsAtEachWidth();
    xorUpdatesResultAndFlagsAtEachWidth();
    notRespectsWidthAndPreservesFlags();
    bitwiseInstructionsSupportMemoryDestinations();
    bitwiseInstructionsPreserveOtherRegisterBits();
    bitwiseInstructionsValidateOperands();
}
