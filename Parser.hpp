
#pragma once

#include <cctype>
#include <charconv>
#include <cstdint>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "Instruction.hpp"

using namespace vm;

namespace parser {

inline std::string trim(std::string_view text) {
    auto start = text.find_first_not_of(" \t\r\n");
    if (start == std::string_view::npos)
        return "";

    auto end = text.find_last_not_of(" \t\r\n");
    return std::string(text.substr(start, end - start + 1));
}

inline std::string uppercase(std::string text) {
    for (char& c : text) {
        c = static_cast<char>(
            std::toupper(static_cast<unsigned char>(c))
        );
    }
    return text;
}

inline Opcode parseOpcode(const std::string& text) {
    const auto op = uppercase(text);

    if (op == "MOV") return Opcode::MOV;
    if (op == "ADD") return Opcode::ADD;
    if (op == "SUB") return Opcode::SUB;
    if (op == "HLT") return Opcode::HLT;

    throw std::runtime_error("Unknown opcode: " + text);
}

inline Operand parseOperand(const std::string& text) {
    const auto token = uppercase(trim(text));

    if (token == "RAX") return RegisterName::RAX;
    if (token == "RBX") return RegisterName::RBX;

    // Parse a non-negative decimal immediate.
    std::uint64_t value{};
    auto [ptr, ec] = std::from_chars(
        token.data(),
        token.data() + token.size(),
        value
    );

    if (ec == std::errc{} &&
        ptr == token.data() + token.size()) {
        return value;
    }

    throw std::runtime_error("Invalid operand: " + text);
}

inline Instruction parseLine(std::string line) {
    // Remove comments.
    if (auto pos = line.find(';'); pos != std::string::npos)
        line.erase(pos);

    line = trim(line);
    if (line.empty())
        throw std::runtime_error("Empty instruction");

    // Separate opcode from its operands.
    auto split = line.find_first_of(" \t");

    std::string opcodeText = line.substr(0, split);
    std::string operandsText =
        split == std::string::npos ? "" : trim(line.substr(split + 1));

    Instruction instruction{
        .opcode = parseOpcode(opcodeText),
        .first = std::nullopt,
        .second = std::nullopt
    };

    if (instruction.opcode == Opcode::HLT) {
        if (!operandsText.empty())
            throw std::runtime_error("HLT takes no operands");
        return instruction;
    }

    // Our current arithmetic instructions require two operands.
    auto comma = operandsText.find(',');
    if (comma == std::string::npos ||
        operandsText.find(',', comma + 1) != std::string::npos) {
        throw std::runtime_error("Expected two operands separated by a comma");
    }

    auto firstText = trim(operandsText.substr(0, comma));
    auto secondText = trim(operandsText.substr(comma + 1));

    instruction.first = parseOperand(firstText);
    instruction.second = parseOperand(secondText);

    // For our subset, destinations must be registers.
    if (!std::holds_alternative<RegisterName>(*instruction.first))
        throw std::runtime_error("Destination must be a register");

    return instruction;
}

inline std::vector<Instruction> parseFile(const std::string& path) {
    std::ifstream file(path);

    if (!file)
        throw std::runtime_error("Cannot open file: " + path);

    std::vector<Instruction> instructions;

    std::string line;
    std::size_t lineNumber = 0;

    while (std::getline(file, line)) {
        ++lineNumber;

        // Ignore comments and empty lines.
        auto content = trim(line.substr(0, line.find(';')));
        if (content.empty())
            continue;

        try {
            instructions.push_back(parseLine(line));
        } catch (const std::exception& e) {
            throw std::runtime_error(
                path + ":" + std::to_string(lineNumber) +
                ": " + e.what()
            );
        }
    }

    if (file.bad())
        throw std::runtime_error("Error reading file: " + path);

    return instructions;
}

} // namespace parser