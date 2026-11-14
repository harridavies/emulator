#include "CPUState.hpp"
#include "TestSupport.hpp"

namespace {

void startsReadyToExecute() {
    // GIVEN a newly constructed CPU
    CPUState cpu;

    // WHEN its initial execution state is inspected
    // THEN it is running at instruction zero with cleared registers and flags
    test::expect(cpu.state_ == State::RUNNING, "CPU must start running");
    test::expect(cpu.ip_ == 0, "Instruction pointer must start at zero");
    for (const auto& reg : cpu.registers_)
        test::expect(reg.read64() == 0, "CPU registers must start at zero");
    test::expect(cpu.carryFlag_ == 0 && cpu.zeroFlag_ == 0 &&
                 cpu.signFlag_ == 0 && cpu.overflowFlag_ == 0, "CPU flags must start cleared");
}

void mapsRegisterNamesToIndependentRegisters() {
    // GIVEN a CPU and its four supported register names
    CPUState cpu;
    const RegisterName names[] = {
        RegisterName::RAX, RegisterName::RBX, RegisterName::RCX, RegisterName::RDX
    };

    // WHEN different values are written through each register's mapped index
    for (std::size_t i = 0; i < numRegisters; ++i)
        cpu.registers_.at(cpu.getRegisterIndex(names[i])).write64(i + 1);

    // THEN each name maps to the correct, independent register
    for (std::size_t i = 0; i < numRegisters; ++i) {
        test::expect(cpu.getRegisterIndex(names[i]) == i, "Incorrect register index");
        test::expect(cpu.registers_[i].read64() == i + 1, "Register mappings overlap");
    }
}

void rejectsUnknownRegisters() {
    // GIVEN a CPU and an invalid register name
    CPUState cpu;
    const auto invalid = static_cast<RegisterName>(99);

    // WHEN its register index is requested
    // THEN the lookup throws
    test::expectThrows<const char*>([&] { (void)cpu.getRegisterIndex(invalid); },
                                   "Invalid register name must be rejected");
}

} // namespace

int main() {
    startsReadyToExecute();
    mapsRegisterNamesToIndependentRegisters();
    rejectsUnknownRegisters();
}
