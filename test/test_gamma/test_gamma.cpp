#include <string>

#include "unity.h"
#include "support/Gamma.h"

void setUp() {}

void tearDown() {}

void test_correct8_endpoints() {
    TEST_ASSERT_EQUAL_UINT8(0, Support::Gamma::correct8(0));
    TEST_ASSERT_EQUAL_UINT8(255, Support::Gamma::correct8(255));
}

// The gamma table compresses the low end: input 1 already maps to 0.
void test_correct8_low_end_darkens() {
    TEST_ASSERT_EQUAL_UINT8(0, Support::Gamma::correct8(1));
    TEST_ASSERT_EQUAL_UINT8(0, Support::Gamma::correct8(14));
}

void test_correct8_known_midpoints() {
    // Pinned against the gammaTable in Gamma.cpp (gamma = 2.2).
    TEST_ASSERT_EQUAL_UINT8(12, Support::Gamma::correct8(64));
    TEST_ASSERT_EQUAL_UINT8(56, Support::Gamma::correct8(128));
    TEST_ASSERT_EQUAL_UINT8(137, Support::Gamma::correct8(192));
}

void test_correct8_is_monotonic() {
    for (int x = 1; x <= 255; x++) {
        TEST_ASSERT_TRUE_MESSAGE(Support::Gamma::correct8(static_cast<uint8_t>(x)) >=
                                     Support::Gamma::correct8(static_cast<uint8_t>(x - 1)),
                                 ("monotonicity violated at x=" + std::to_string(x)).c_str());
    }
}

// correct32 applies the table per channel, preserving the 0xRRGGBB layout.
void test_correct32_applies_each_channel() {
    TEST_ASSERT_EQUAL_HEX32(0x380C89, Support::Gamma::correct32(0x8040C0));
}

void test_correct32_black_and_white() {
    TEST_ASSERT_EQUAL_HEX32(0x000000, Support::Gamma::correct32(0x000000));
    TEST_ASSERT_EQUAL_HEX32(0xFFFFFF, Support::Gamma::correct32(0xFFFFFF));
}

// The inverse table is the identity for every 8-bit input.
void test_uncorrect8_is_identity() {
    for (int x = 0; x <= 255; x++) {
        TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(x), Support::Gamma::uncorrect8(static_cast<uint8_t>(x)));
    }
}

void test_uncorrect32_is_identity() {
    TEST_ASSERT_EQUAL_HEX32(0x8040C0, Support::Gamma::uncorrect32(0x8040C0));
    TEST_ASSERT_EQUAL_HEX32(0xFFAABBCC, Support::Gamma::uncorrect32(0xFFAABBCC));
}

int runUnityTests() {
    UNITY_BEGIN();
    RUN_TEST(test_correct8_endpoints);
    RUN_TEST(test_correct8_low_end_darkens);
    RUN_TEST(test_correct8_known_midpoints);
    RUN_TEST(test_correct8_is_monotonic);
    RUN_TEST(test_correct32_applies_each_channel);
    RUN_TEST(test_correct32_black_and_white);
    RUN_TEST(test_uncorrect8_is_identity);
    RUN_TEST(test_uncorrect32_is_identity);
    return UNITY_END();
}

int main() {
    return runUnityTests();
}
