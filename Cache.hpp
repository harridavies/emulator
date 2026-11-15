#include <cstddef>
#include <cstdint>
#include <cstring>
#include <array>
#include "Instruction.hpp"

enum class WritePolicy {
    WriteBack,
    WriteThrough
};

template <
    std::size_t NumSets, 
    std::size_t Associativity, 
    std::size_t LineSize, 
    typename LowerLevel,
    WritePolicy writePolicy = WritePolicy::WriteBack>
class Cache {
public:
    struct CacheLine {
        std::array<std::byte, LineSize> data_;
        std::uint64_t tag_{};
        bool valid_{false};
        bool dirty_{false};
    };
    using CacheSet = std::array<CacheLine, Associativity>;

    explicit Cache(LowerLevel& lower) : lower_(lower) {}

    // find the data 
    // - search all levels to find the data
    // - if found in lower level, then copy to the higher levels
    // - throw if no data found in any levels
    std::uint64_t read(std::uint64_t address, OperandWidth width) {
        std::size_t blockOffset = address % LineSize;
        std::size_t numBytes{};

        switch (width) {
            case OperandWidth::Byte:  numBytes = 1; break;
            case OperandWidth::Word:  numBytes = 2; break;
            case OperandWidth::DWord: numBytes = 4; break;
            case OperandWidth::QWord: numBytes = 8; break;
            default: throw "invalid operand width";
        }

        CacheLine& line = findLine(address);
        std::uint64_t value{};

        std::size_t firstBytes = std::min(numBytes, LineSize - blockOffset);
        
        std::memcpy(&value, line.data_.data() + blockOffset, firstBytes);
        
        std::size_t remainingBytes = numBytes - firstBytes;

        if (remainingBytes > 0) {
            CacheLine& line2 = findLine(address + firstBytes);
            auto* destination = reinterpret_cast<std::byte*>(&value);

            std::memcpy(destination + firstBytes, line2.data_.data() , remainingBytes);
        }
        
        return value;
    }

    // find line in this cache level (go to lower levels if not found)
    CacheLine& findLine(std::uint64_t address) {
        std::size_t blockIndex = address / LineSize;
        std::size_t setIndex = blockIndex % NumSets;
        std::uint64_t tag = blockIndex / NumSets;

        for (auto& line : sets_[setIndex]) {
            if (line.valid_ && line.tag_ == tag) {
                return line;
            }
        } 

        return loadLine(address);
    }
    
    // find line in lower levels
    CacheLine& loadLine(std::uint64_t address) {
        std::size_t blockIndex = address / LineSize;
        std::size_t setIndex = blockIndex % NumSets;

        CacheLine* destination = nullptr;

        for (auto& line : sets_[setIndex]) {
            if (!line.valid_) {
                destination = &line;
                break;
            }
        }

        if (!destination) {
            // for now just evict first line, eventually add LRU cache!
            auto& victim  = sets_[setIndex][0];

            if (victim.valid_ && victim.dirty_) {
                writeBack(victim, setIndex);
            }
            destination = &victim;
        }

        if (!destination) {
            throw "cache set full: eviction not implemented";
        }

        std::uint64_t blockAddress = blockIndex * LineSize;
        lower_.readBlock(blockAddress, destination->data_.data());

        destination->tag_ = blockIndex / NumSets;
        destination->valid_ = true;
        destination->dirty_ = false;

        return *destination;
    }

    // connection to lower levels
    void readBlock(std::uint64_t address, std::byte* destination) {
        if (address % LineSize != 0) {
            throw "unaligned block read";
        }

        CacheLine& line = findLine(address);
        std::memcpy(destination, (line.data_.data()), LineSize);
    }

    void writeBlock(std::uint64_t address, const std::byte* source) {
        if (address % LineSize != 0) {
            throw "unaligned block write";
        }

        CacheLine& line = findLine(address);
        std::memcpy(line.data_.data(), source, LineSize);
        line.dirty_ = true;
    }


    void write(std::uint64_t address, std::uint64_t value, OperandWidth width) {

        std::size_t blockOffset = address % LineSize;
        uint64_t numBytes{};

        switch (width) {
            case OperandWidth::Byte:  numBytes = 1; break;
            case OperandWidth::Word:  numBytes = 2; break;
            case OperandWidth::DWord: numBytes = 4; break;
            case OperandWidth::QWord: numBytes = 8; break;
            default: throw "invalid operand width";
        }

        std::size_t firstBytes = std::min(numBytes, LineSize - blockOffset);

        CacheLine& line = findLine(address);

        std::memcpy(line.data_.data() + blockOffset, &value, firstBytes);
        line.dirty_ = true;

        std::size_t remainingBytes = numBytes - firstBytes;

        if (remainingBytes > 0) {
            CacheLine& line2 = findLine(address + firstBytes);
            const auto* source = reinterpret_cast<std::byte*>(&value);
            std::memcpy(line2.data_.data(), source + firstBytes, remainingBytes);
            
            line2.dirty_ = true;
        }
    }

    void writeBack(const CacheLine& victim, std::size_t setIndex) {
        const std::uint64_t blockIndex = victim.tag_ * NumSets + setIndex;
        const std::uint64_t blockAddress = blockIndex * LineSize;

        lower_.writeBlock(blockAddress, victim.data_.data());
    }

private:
    std::array<CacheSet, NumSets> sets_;
    LowerLevel& lower_;
};