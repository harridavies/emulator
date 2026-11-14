#include "Register.hpp"
#include "TestSupport.hpp"

namespace {

void startsAtZero() {
    // GIVEN a newly constructed register
    Register reg;

    // WHEN its full value is read
    // THEN the value is zero
    test::expect(reg.read64() == 0, "New registers must start at zero");
}

void readsEachRegisterView() {
    // GIVEN a register containing a known 64-bit value
    Register reg;
    reg.write64(0x1122334455667788ULL);

    // WHEN its different views are read
    // THEN each view returns the corresponding bits
    test::expect(reg.read8() == 0x88, "Incorrect low byte");
    test::expect(reg.readHigh8() == 0x77, "Incorrect high byte");
    test::expect(reg.read16() == 0x7788, "Incorrect word");
    test::expect(reg.read32() == 0x55667788, "Incorrect double word");
    test::expect(reg.read64() == 0x1122334455667788ULL, "Incorrect full register");
}

void smallWritesPreserveOtherBits() {
    // GIVEN a register with populated upper bits
    Register reg;
    reg.write64(0x1122334455667788ULL);

    // WHEN its low byte, high byte, and word are written
    // THEN each write preserves the bits outside that view
    reg.write8(0xAA);
    test::expect(reg.read64() == 0x11223344556677AAULL, "Byte write changed other bits");
    reg.writeHigh8(0xBB);
    test::expect(reg.read64() == 0x112233445566BBAAULL, "High-byte write changed other bits");
    reg.write16(0xCCDD);
    test::expect(reg.read64() == 0x112233445566CCDDULL, "Word write changed upper bits");
}

void doubleWordWriteClearsUpperBits() {
    // GIVEN a register with all bits set
    Register reg;
    reg.write64(0xFFFFFFFFFFFFFFFFULL);

    // WHEN a 32-bit value is written
    reg.write32(0x12345678);

    // THEN the upper 32 bits are cleared
    test::expect(reg.read64() == 0x12345678ULL, "Double-word write must clear upper bits");
}

} // namespace

int main() {
    startsAtZero();
    readsEachRegisterView();
    smallWritesPreserveOtherBits();
    doubleWordWriteClearsUpperBits();
}
