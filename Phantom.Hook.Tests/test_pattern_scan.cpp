#include <gtest/gtest.h>

#include <cstdint>

#include "hook/pattern_scan.h"

using namespace phantom::hook;

// ===========================================================================
// PatternScanner tests
// ===========================================================================

TEST(PatternScanner, FindExactPattern) {
    uint8_t data[] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
    MemoryRegion region(MemoryHandle(reinterpret_cast<void*>(data)), sizeof(data));

    auto result = PatternScanner::scan(region, "22 33 44");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().as<const uint8_t*>(), &data[2]);
}

TEST(PatternScanner, FindPatternWithWildcard) {
    uint8_t data[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE};
    MemoryRegion region(MemoryHandle(reinterpret_cast<void*>(data)), sizeof(data));

    auto result = PatternScanner::scan(region, "BB ?? DD");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().as<const uint8_t*>(), &data[1]);
}

TEST(PatternScanner, PatternNotFound) {
    uint8_t data[] = {0x00, 0x11, 0x22, 0x33, 0x44};
    MemoryRegion region(MemoryHandle(reinterpret_cast<void*>(data)), sizeof(data));

    auto result = PatternScanner::scan(region, "FF EE");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ScanError::PatternNotFound);
}

TEST(PatternScanner, EmptyPatternFails) {
    uint8_t data[] = {0x00, 0x11};
    MemoryRegion region(MemoryHandle(reinterpret_cast<void*>(data)), sizeof(data));

    auto result = PatternScanner::scan(region, "");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ScanError::InvalidPattern);
}

TEST(PatternScanner, InvalidHexFails) {
    uint8_t data[] = {0x00, 0x11};
    MemoryRegion region(MemoryHandle(reinterpret_cast<void*>(data)), sizeof(data));

    auto result = PatternScanner::scan(region, "ZZ");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ScanError::InvalidPattern);
}

TEST(PatternScanner, FindFirstOccurrence) {
    uint8_t data[] = {0xAA, 0xBB, 0xCC, 0xAA, 0xBB, 0xCC};
    MemoryRegion region(MemoryHandle(reinterpret_cast<void*>(data)), sizeof(data));

    auto result = PatternScanner::scan(region, "AA BB CC");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().as<const uint8_t*>(), &data[0]);
}

TEST(PatternScanner, PatternAtEndOfRegion) {
    uint8_t data[] = {0x00, 0x00, 0x00, 0xDE, 0xAD};
    MemoryRegion region(MemoryHandle(reinterpret_cast<void*>(data)), sizeof(data));

    auto result = PatternScanner::scan(region, "DE AD");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().as<const uint8_t*>(), &data[3]);
}
