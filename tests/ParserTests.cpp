#include "Parser.hpp"
#include "TestSupport.hpp"

#include <filesystem>
#include <limits>

namespace {

void normalisesOperandOrder() {
    // GIVEN equivalent Intel and AT&T instructions
    // WHEN both instructions are parsed
    const auto intel = parser::parseLine("MoV rax, 10 ; inline comment");
    const auto att = parser::parseLine("movq $10, %rax");

    // THEN both store the immediate source first and register destination second
    for (const auto* instruction : {&intel, &att}) {
        test::expect(instruction->opcode_ == Opcode::MOV, "Incorrect MOV opcode");
        test::expect(instruction->width_ == OperandWidth::QWord, "Incorrect MOV width");
        test::expect(std::get<std::uint64_t>(instruction->first_.value()) == 10, "Incorrect source operand");
        test::expect(std::get<RegisterView>(instruction->second_.value()).register_ == RegisterName::RAX,
                     "Incorrect destination operand");
    }
}

void parsesRegisterViewsAndNegativeImmediates() {
    // GIVEN an instruction targeting a high-byte register
    // WHEN a negative immediate is parsed
    const auto instruction = parser::parseLine("mov ah, -1");
    const auto view = std::get<RegisterView>(instruction.second_.value());

    // THEN its register view has byte width and bit offset eight
    test::expect(view.register_ == RegisterName::RAX && view.width_ == OperandWidth::Byte &&
                 view.bitOffset_ == 8, "Incorrect high-byte register view");
    test::expect(instruction.width_ == OperandWidth::Byte, "Width must be inferred from AH");
    test::expect(std::get<std::uint64_t>(instruction.first_.value()) ==
                 std::numeric_limits<std::uint64_t>::max(), "Negative immediate must use two's complement");
}

void parsesSizedMemoryAndNumericBases() {
    // GIVEN a memory destination with an explicit width and hexadecimal address
    // WHEN an instruction with a binary immediate is parsed
    const auto instruction = parser::parseLine("mov DWORD PTR [0x10], 0b1010");

    // THEN the width, address, and immediate are preserved
    test::expect(instruction.width_ == OperandWidth::DWord, "Incorrect memory width");
    test::expect(std::get<MemoryAddress>(instruction.second_.value()).address_ == 16, "Incorrect address");
    test::expect(std::get<std::uint64_t>(instruction.first_.value()) == 10, "Incorrect binary immediate");
}

void parsesUnaryJumpAndHaltInstructions() {
    // GIVEN instructions requiring one, one, and zero operands respectively
    // WHEN they are parsed
    const auto unary = parser::parseLine("inc ecx");
    const auto jump = parser::parseLine("jnz 2");
    const auto halt = parser::parseLine("hlt");

    // THEN each opcode has the correct operands and width
    test::expect(unary.opcode_ == Opcode::INC && unary.width_ == OperandWidth::DWord &&
                 unary.first_.has_value() && !unary.second_, "Incorrect unary instruction");
    test::expect(jump.opcode_ == Opcode::JNZ && !jump.second_ &&
                 std::get<std::uint64_t>(jump.first_.value()) == 2, "Incorrect jump instruction");
    test::expect(halt.opcode_ == Opcode::HLT && !halt.first_ && !halt.second_, "HLT must have no operands");
}

void rejectsInvalidInstructions() {
    // GIVEN malformed instructions, mismatched widths, and invalid operand values
    const char* invalid[] = {
        "unknown rax, 1", "mov rax", "hlt rax", "mov 10, rax",
        "mov eax, rbx", "mov al, 256", "mov [16], 1", "mov rax, [-1]"
    };

    // WHEN each instruction is parsed
    // THEN the parser reports an error
    for (const auto* line : invalid)
        test::expectThrows<std::runtime_error>([&] { (void)parser::parseLine(line); },
                                              "Invalid assembly instruction was accepted");
}

void readsAssemblyFiles() {
    // GIVEN an assembly file containing comments, blank lines, and instructions
    const auto path = std::filesystem::path(__FILE__).parent_path() / "fixtures" / "parser.asm";

    // WHEN the file is parsed
    const auto instructions = parser::parseFile(path.string());

    // THEN only the instructions are returned in their original order
    test::expect(instructions.size() == 3, "Comments and blank lines must be skipped");
    test::expect(instructions[0].opcode_ == Opcode::MOV && instructions[1].opcode_ == Opcode::INC &&
                 instructions[2].opcode_ == Opcode::HLT, "Incorrect instruction order");
}

} // namespace

int main() {
    normalisesOperandOrder();
    parsesRegisterViewsAndNegativeImmediates();
    parsesSizedMemoryAndNumericBases();
    parsesUnaryJumpAndHaltInstructions();
    rejectsInvalidInstructions();
    readsAssemblyFiles();
}
