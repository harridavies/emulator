#include "MainMemory.hpp"
#include "TestSupport.hpp"
#include "Cache.hpp"
#include <utility>

namespace {

void startsAtZero() {
    // GIVEN freshly allocated memory
    MainMemory memory;

    // WHEN its first and last bytes are read
    // THEN both are zero
    test::expect(memory.read8(0) == 0, "First byte must start at zero");
    test::expect(memory.read8(DRAM_SIZE - 1) == 0, "Last byte must start at zero");
}

void readsAndWritesEachWidth() {
    // GIVEN memory with space for each supported width
    MainMemory memory;

    // WHEN values of each width are written, including an unaligned 64-bit value
    memory.write8(0, 0xAB);
    memory.write16(2, 0x1234);
    memory.write32(4, 0x12345678);
    memory.write64(9, 0x1122334455667788ULL);

    // THEN the values round-trip and the bytes use little-endian order
    test::expect(memory.read8(0) == 0xAB, "Byte round-trip failed");
    test::expect(memory.read16(2) == 0x1234, "Word round-trip failed");
    test::expect(memory.read32(4) == 0x12345678, "Double-word round-trip failed");
    test::expect(memory.read64(9) == 0x1122334455667788ULL, "Quad-word round-trip failed");
    test::expect(memory.read8(9) == 0x88 && memory.read8(16) == 0x11,
                 "Memory must store the least significant byte first");
}

void checksMemoryBoundaries() {
    // GIVEN the last valid address for each access width
    MainMemory memory;

    // WHEN values are written and read at those addresses
    // THEN they succeed, while accesses one byte beyond them throw
    memory.write8(DRAM_SIZE - 1, 1);
    test::expect(memory.read8(DRAM_SIZE - 1) == 1, "Last byte must be accessible");
    memory.write16(DRAM_SIZE - 2, 2);
    test::expect(memory.read16(DRAM_SIZE - 2) == 2, "Last word must be accessible");
    memory.write32(DRAM_SIZE - 4, 3);
    test::expect(memory.read32(DRAM_SIZE - 4) == 3, "Last double word must be accessible");
    memory.write64(DRAM_SIZE - 8, 4);
    test::expect(memory.read64(DRAM_SIZE - 8) == 4, "Last quad word must be accessible");
    test::expectThrows<std::runtime_error>([&] { (void)memory.read8(DRAM_SIZE); }, "Out-of-range byte read accepted");
    test::expectThrows<std::runtime_error>([&] { memory.write8(DRAM_SIZE, 0); }, "Out-of-range byte write accepted");
    test::expectThrows<std::runtime_error>([&] { (void)memory.read16(DRAM_SIZE - 1); }, "Out-of-range word read accepted");
    test::expectThrows<std::runtime_error>([&] { memory.write16(DRAM_SIZE - 1, 0); }, "Out-of-range word write accepted");
    test::expectThrows<std::runtime_error>([&] { (void)memory.read32(DRAM_SIZE - 3); }, "Out-of-range double-word read accepted");
    test::expectThrows<std::runtime_error>([&] { memory.write32(DRAM_SIZE - 3, 0); }, "Out-of-range double-word write accepted");
    test::expectThrows<std::runtime_error>([&] { (void)memory.read64(DRAM_SIZE - 7); }, "Out-of-range quad-word read accepted");
    test::expectThrows<std::runtime_error>([&] { memory.write64(DRAM_SIZE - 7, 0); }, "Out-of-range quad-word write accepted");
}

void movesTransferMemoryOwnership() {
    // GIVEN memory containing a value
    MainMemory original;
    original.write64(0, 42);

    // WHEN ownership is transferred by move construction and move assignment
    MainMemory moved(std::move(original));
    MainMemory assigned;
    assigned = std::move(moved);

    // THEN the destination retains the value and moved-from memory rejects access
    test::expect(assigned.read64(0) == 42, "Move must preserve memory contents");
    test::expectThrows<std::runtime_error>([&] { (void)original.read8(0); }, "Moved-from memory accepted a read");
    test::expectThrows<std::runtime_error>([&] { moved.write8(0, 1); }, "Moved-from memory accepted a write");
}

} // namespace

int main() {
    startsAtZero();
    readsAndWritesEachWidth();
    checksMemoryBoundaries();
    movesTransferMemoryOwnership();
}
