#include <iostream>
#include <exception>

#include "Parser.hpp"
#include "Instruction.hpp"
#include "VirtualMachine.hpp"

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <assembly-file>\n";
        return 1;
    }

    try {
        auto instructions = parser::parseFile(argv[1]);

        std::cout << "Successfully parsed "
                  << instructions.size()
                  << " instructions\n";

        vm::VirtualMachine vm(instructions);

        vm.run();
        vm.state().debugRegisters();
        vm.state().status();

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }

    return 0;
}
