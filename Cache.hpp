#include <cstddef>
#include <cstdint>
#include <cstring>
#include <array>
#include "Instruction.hpp"

template <
    std::size_t NumSets, 
    std::size_t Associativity, 
    std::size_t LineSize, 
    typename LowerLevel>
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

        if (numBytes > LineSize - blockOffset) {
            throw "read crosses cache line boundary";
        }

        CacheLine& line = findLine(address);

        std::uint64_t value{};
        std::memcpy(&value, &line.data_[blockOffset], numBytes);
        
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

        // if (!destination) {
        //     // for now just evict first line, eventually add LRU cache!
        //     destination = &(sets_[setIndex][0]);
        // }

        if (!destination) {
            throw "cache set full: eviction not implemented";
        }

        std::uint64_t blockAddress = blockIndex * LineSize;
        lower_.readBlock(blockAddress, destination->data_.data(), LineSize);

        destination->tag_ = blockIndex / NumSets;
        destination->valid_ = true;
        destination->dirty_ = false;

        return *destination;
    }

    // connection to lower levels
    void readBlock(std::uint64_t address, std::byte* destination) {
        CacheLine& line = findLine(address);
        std::memcpy(destination, (line.data_.data()), LineSize);
    }

private:
    std::array<CacheSet, NumSets> sets_;
    LowerLevel& lower_;
};