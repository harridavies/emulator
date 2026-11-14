#pragma once

#include <array>
#include <cctype>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "Instruction.hpp"

namespace parser {

// Plain assembly uses destination, source, e.g. mov eax, [16]. AT&T syntax
// uses source, destination, e.g. movl 16, %eax. A mnemonic size suffix or
// %/$ operand prefix selects AT&T syntax. Only absolute memory addresses and
// numeric, zero-based instruction indices for jumps are supported.
// Instructions emitted for the VM always store source first, destination second.

inline std::string trim(std::string_view text) {
    const auto start = text.find_first_not_of(" \t\r\n");
    if (start == std::string_view::npos)
        return "";
    const auto end = text.find_last_not_of(" \t\r\n");
    return std::string(text.substr(start, end - start + 1));
}

inline std::string uppercase(std::string text) {
    for (char& c : text)
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return text;
}

namespace detail {

struct Mnemonic {
    Opcode opcode;
    std::optional<OperandWidth> width;
};

inline bool isJump(Opcode opcode) {
    switch (opcode) {
        case Opcode::JMP: case Opcode::JE: case Opcode::JZ:
        case Opcode::JNE: case Opcode::JNZ: case Opcode::JC:
        case Opcode::JNC: case Opcode::JG: case Opcode::JL:
            return true;
        default:
            return false;
    }
}

inline Mnemonic parseMnemonic(const std::string& text) {
    const auto token = uppercase(trim(text));
    static constexpr std::pair<std::string_view, Opcode> opcodes[] = {
        {"MOV", Opcode::MOV}, {"ADD", Opcode::ADD}, {"SUB", Opcode::SUB},
        {"INC", Opcode::INC}, {"DEC", Opcode::DEC}, {"JMP", Opcode::JMP},
        {"CMP", Opcode::CMP}, {"AND", Opcode::AND}, {"OR", Opcode::OR},
        {"XOR", Opcode::XOR}, {"NOT", Opcode::NOT}, {"JE", Opcode::JE},
        {"JZ", Opcode::JZ}, {"JNE", Opcode::JNE}, {"JNZ", Opcode::JNZ},
        {"JC", Opcode::JC}, {"JNC", Opcode::JNC}, {"JG", Opcode::JG},
        {"JL", Opcode::JL}, {"HLT", Opcode::HLT}
    };
    for (const auto& [name, opcode] : opcodes)
        if (token == name)
            return {opcode, std::nullopt};

    if (!token.empty()) {
        std::optional<OperandWidth> width;
        switch (token.back()) {
            case 'B': width = OperandWidth::Byte; break;
            case 'W': width = OperandWidth::Word; break;
            case 'L': width = OperandWidth::DWord; break;
            case 'Q': width = OperandWidth::QWord; break;
        }
        if (width) {
            const auto base = token.substr(0, token.size() - 1);
            for (const auto& [name, opcode] : opcodes)
                if (base == name && opcode != Opcode::HLT &&
                    (!isJump(opcode) || opcode == Opcode::JMP))
                    return {opcode, width};
        }
    }
    throw std::runtime_error("Unknown opcode: " + text);
}

inline std::optional<RegisterView> parseRegister(std::string token) {
    if (!token.empty() && token.front() == '%')
        token.erase(0, 1);
    static constexpr std::array<std::array<std::string_view, 5>, 4> names{{
        {{"RAX", "EAX", "AX", "AL", "AH"}},
        {{"RBX", "EBX", "BX", "BL", "BH"}},
        {{"RCX", "ECX", "CX", "CL", "CH"}},
        {{"RDX", "EDX", "DX", "DL", "DH"}}
    }};
    static constexpr std::array<RegisterName, 4> registers{
        RegisterName::RAX, RegisterName::RBX, RegisterName::RCX, RegisterName::RDX
    };
    static constexpr std::array<OperandWidth, 5> widths{
        OperandWidth::QWord, OperandWidth::DWord, OperandWidth::Word,
        OperandWidth::Byte, OperandWidth::Byte
    };
    for (std::size_t i = 0; i < names.size(); ++i)
        for (std::size_t j = 0; j < names[i].size(); ++j)
            if (token == names[i][j])
                return RegisterView{registers[i], widths[j], j == 4 ? 8ULL : 0ULL};
    return std::nullopt;
}

struct Number {
    std::uint64_t magnitude;
    bool negative;
};

inline Number parseNumber(std::string token, bool allowNegative) {
    const auto original = token;
    bool negative = false;
    if (!token.empty() && (token.front() == '+' || token.front() == '-')) {
        negative = token.front() == '-';
        token.erase(0, 1);
    }
    if (negative && !allowNegative)
        throw std::runtime_error("Address or jump target must be non-negative: " + original);

    int base = 10;
    if (token.size() >= 2 && token[0] == '0') {
        if (token[1] == 'X') { base = 16; token.erase(0, 2); }
        else if (token[1] == 'B') { base = 2; token.erase(0, 2); }
    }
    std::uint64_t value{};
    const auto [ptr, ec] = std::from_chars(token.data(), token.data() + token.size(), value, base);
    if (token.empty() || ec != std::errc{} || ptr != token.data() + token.size())
        throw std::runtime_error("Invalid integer: " + original);
    if (negative && value > (std::uint64_t{1} << 63))
        throw std::runtime_error("Negative immediate is outside the 64-bit range: " + original);
    return {value, negative};
}

struct ParsedOperand {
    Operand value;
    std::optional<OperandWidth> width;
    std::optional<Number> immediate;
};

inline ParsedOperand parseOperandText(const std::string& text, bool att, bool jump) {
    auto token = uppercase(trim(text));
    if (token.empty())
        throw std::runtime_error("Missing operand");

    // AT&T '*' marks an indirect jump; the VM can read a register or memory.
    bool indirect = false;
    if (att && jump && token.front() == '*') {
        indirect = true;
        token = trim(token.substr(1));
        if (token.empty())
            throw std::runtime_error("Missing indirect jump target");
    }

    std::optional<OperandWidth> width;
    const auto split = token.find_first_of(" \t");
    const auto qualifier = token.substr(0, split);
    if (qualifier == "BYTE") width = OperandWidth::Byte;
    else if (qualifier == "WORD") width = OperandWidth::Word;
    else if (qualifier == "DWORD") width = OperandWidth::DWord;
    else if (qualifier == "QWORD") width = OperandWidth::QWord;
    if (width) {
        token = split == std::string::npos ? "" : trim(token.substr(split + 1));
        if (token == "PTR" || token.substr(0, 4) == "PTR " || token.substr(0, 4) == "PTR\t")
            token = trim(token.substr(3));
        if (token.empty() || token.front() != '[')
            throw std::runtime_error("Memory size must precede an absolute address in brackets");
    }

    if (auto view = parseRegister(token)) {
        if (att && token.front() != '%')
            throw std::runtime_error("AT&T registers require a % prefix: " + text);
        return {*view, view->width_, std::nullopt};
    }
    if (!token.empty() && token.front() == '%')
        throw std::runtime_error("Unknown register: " + text);

    if (!token.empty() && token.front() == '[') {
        if (token.back() != ']')
            throw std::runtime_error("Unclosed memory address: " + text);
        const auto address = parseNumber(trim(token.substr(1, token.size() - 2)), false);
        return {MemoryAddress{address.magnitude}, width, std::nullopt};
    }

    const bool explicitImmediate = !token.empty() && token.front() == '$';
    if (explicitImmediate) {
        if (indirect)
            throw std::runtime_error("Indirect jump target cannot be an immediate");
        token.erase(0, 1);
    }
    const bool memory = att && !explicitImmediate && (!jump || indirect);
    const auto number = parseNumber(token, !memory && !jump);
    if (memory)
        return {MemoryAddress{number.magnitude}, std::nullopt, std::nullopt};
    const auto value = number.negative ? std::uint64_t{0} - number.magnitude : number.magnitude;
    return {value, std::nullopt, number};
}

inline void mergeWidth(std::optional<OperandWidth>& width, std::optional<OperandWidth> operandWidth) {
    if (!operandWidth)
        return;
    if (width && width != operandWidth)
        throw std::runtime_error("Operand widths do not match");
    width = operandWidth;
}

inline void validateImmediate(const ParsedOperand& operand, OperandWidth width) {
    if (!operand.immediate)
        return;
    const auto number = *operand.immediate;
    const auto bits = static_cast<unsigned>(width);
    const auto maximum = number.negative ? (std::uint64_t{1} << (bits - 1)) :
        (bits == 64 ? std::numeric_limits<std::uint64_t>::max() : (std::uint64_t{1} << bits) - 1);
    if (number.magnitude > maximum)
        throw std::runtime_error("Immediate does not fit the instruction width");
}

} // namespace detail

inline Opcode parseOpcode(const std::string& text) {
    return detail::parseMnemonic(text).opcode;
}

inline Operand parseOperand(const std::string& text) {
    // Standalone operands retain the original API: bare numbers are immediates.
    return detail::parseOperandText(text, false, false).value;
}

inline Instruction parseLine(std::string line) {
    if (const auto pos = line.find(';'); pos != std::string::npos)
        line.erase(pos);
    line = trim(line);
    if (line.empty())
        throw std::runtime_error("Empty instruction");

    const auto split = line.find_first_of(" \t");
    const auto mnemonic = detail::parseMnemonic(line.substr(0, split));
    const auto operandsText = split == std::string::npos ? "" : trim(line.substr(split + 1));
    const bool jump = detail::isJump(mnemonic.opcode);
    const bool unary = jump || mnemonic.opcode == Opcode::INC ||
        mnemonic.opcode == Opcode::DEC || mnemonic.opcode == Opcode::NOT;
    const bool att = mnemonic.width.has_value() || operandsText.find_first_of("%$*") != std::string::npos;

    Instruction instruction{mnemonic.opcode, OperandWidth::QWord, std::nullopt, std::nullopt};
    if (mnemonic.opcode == Opcode::HLT) {
        if (!operandsText.empty())
            throw std::runtime_error("HLT takes no operands");
        return instruction;
    }

    const auto comma = operandsText.find(',');
    if (unary && (operandsText.empty() || comma != std::string::npos))
        throw std::runtime_error("Expected exactly one operand");
    if (!unary && (comma == std::string::npos || operandsText.find(',', comma + 1) != std::string::npos))
        throw std::runtime_error("Expected two operands separated by a comma");

    auto first = detail::parseOperandText(unary ? operandsText : operandsText.substr(0, comma), att, jump);
    std::optional<detail::ParsedOperand> second;
    if (!unary) {
        second = detail::parseOperandText(operandsText.substr(comma + 1), att, false);
        if (!att)
            std::swap(first, *second);
    }

    // Every data operation needs a writable destination, including CMP's
    // non-immediate comparison operand. Jumps instead read their sole operand.
    const auto& destination = second ? *second : first;
    if (!jump && std::holds_alternative<std::uint64_t>(destination.value))
        throw std::runtime_error("Destination must be a register or memory address");

    auto width = mnemonic.width;
    detail::mergeWidth(width, first.width);
    if (second)
        detail::mergeWidth(width, second->width);
    if (!width && !jump)
        throw std::runtime_error("Ambiguous memory width: use BYTE, WORD, DWORD, QWORD or a mnemonic size suffix");
    instruction.width_ = width.value_or(OperandWidth::QWord);
    detail::validateImmediate(first, instruction.width_);
    if (second)
        detail::validateImmediate(*second, instruction.width_);
    instruction.first_ = first.value;
    if (second)
        instruction.second_ = second->value;
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
        if (trim(line.substr(0, line.find(';'))).empty())
            continue;
        try {
            instructions.push_back(parseLine(line));
        } catch (const std::exception& e) {
            throw std::runtime_error(path + ":" + std::to_string(lineNumber) + ": " + e.what());
        }
    }
    if (file.bad())
        throw std::runtime_error("Error reading file: " + path);
    return instructions;
}

} // namespace parser
