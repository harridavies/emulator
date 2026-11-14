#include <iostream>
#include <exception>

#include "Parser.hpp"
#include "Instruction.hpp"

int main() {
    try {
        auto instructions = parser::parseFile("test.asm");

        std::cout << "Successfully parsed "
                  << instructions.size()
                  << " instructions\n";

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }

    return 0;
}