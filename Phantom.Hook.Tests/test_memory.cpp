#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>

#include "hook/memory.h"

using namespace phantom::hook;

// ===========================================================================
// MemoryHandle tests
// ===========================================================================

TEST(MemoryHandle, DefaultIsInvalid) {
    MemoryHandle h;
    EXPECT_FALSE(h.valid());
    EXPECT_EQ(h.address(), 0u);
}

TEST(MemoryHandle, ConstructFromAddress) {
    MemoryHandle h(static_cast<uintptr_t>(0xDEAD));
    EXPECT_TRUE(h.valid());
    EXPECT_EQ(h.address(), 0xDEADu);
}

TEST(MemoryHandle, AddOffset) {
    MemoryHandle h(static_cast<uintptr_t>(0x1000));
    auto result = h.add(0x50);
    EXPECT_EQ(result.address(), 0x1050u);
}

TEST(MemoryHandle, SubOffset) {
    MemoryHandle h(static_cast<uintptr_t>(0x1050));
    auto result = h.sub(0x50);
    EXPECT_EQ(result.address(), 0x1000u);
}

TEST(MemoryHandle, ComparisonOperators) {
    MemoryHandle a(static_cast<uintptr_t>(0x1000));
    MemoryHandle b(static_cast<uintptr_t>(0x2000));
    MemoryHandle c(static_cast<uintptr_t>(0x1000));

    EXPECT_EQ(a, c);
    EXPECT_NE(a, b);
    EXPECT_LT(a, b);
    EXPECT_GT(b, a);
    EXPECT_LE(a, c);
    EXPECT_GE(b, a);
}

TEST(MemoryHandle, RipRelativeResolution) {
    // Create a buffer: [int32_t offset] where offset points forward by 0x100
    // RIP resolves to: address_of_buffer + offset + 4
    alignas(int32_t) uint8_t buffer[8] = {};
    int32_t offset = 0x100;
    std::memcpy(buffer, &offset, sizeof(offset));

    MemoryHandle h(reinterpret_cast<void*>(buffer));
    auto resolved = h.rip();

    auto expected = reinterpret_cast<uintptr_t>(buffer) + offset + 4;
    EXPECT_EQ(resolved.address(), expected);
}

// ===========================================================================
// MemoryRegion tests
// ===========================================================================

TEST(MemoryRegion, ContainsInside) {
    MemoryRegion region(MemoryHandle(static_cast<uintptr_t>(0x1000)), 0x100);
    EXPECT_TRUE(region.contains(MemoryHandle(static_cast<uintptr_t>(0x1050))));
}

TEST(MemoryRegion, ContainsBoundary) {
    MemoryRegion region(MemoryHandle(static_cast<uintptr_t>(0x1000)), 0x100);
    // Base address is inside
    EXPECT_TRUE(region.contains(MemoryHandle(static_cast<uintptr_t>(0x1000))));
    // End address (base + size) is outside (half-open interval)
    EXPECT_FALSE(region.contains(MemoryHandle(static_cast<uintptr_t>(0x1100))));
}

TEST(MemoryRegion, ContainsOutside) {
    MemoryRegion region(MemoryHandle(static_cast<uintptr_t>(0x1000)), 0x100);
    EXPECT_FALSE(region.contains(MemoryHandle(static_cast<uintptr_t>(0x900))));
    EXPECT_FALSE(region.contains(MemoryHandle(static_cast<uintptr_t>(0x2000))));
}

TEST(MemoryRegion, AsBytes) {
    uint8_t data[] = { 0xAA, 0xBB, 0xCC, 0xDD };
    MemoryRegion region(MemoryHandle(reinterpret_cast<void*>(data)), sizeof(data));
    auto bytes = region.as_bytes();

    ASSERT_EQ(bytes.size(), sizeof(data));
    EXPECT_EQ(bytes[0], 0xAA);
    EXPECT_EQ(bytes[1], 0xBB);
    EXPECT_EQ(bytes[2], 0xCC);
    EXPECT_EQ(bytes[3], 0xDD);
}

// ===========================================================================
// Module tests
// ===========================================================================

TEST(Module, NonexistentModuleReturnsError) {
    auto result = Module::from_name("nonexistent_module_12345.dll");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), MemoryError::ModuleNotFound);
}

TEST(Module, Kernel32Exists) {
    auto result = Module::from_name("kernel32.dll");
    ASSERT_TRUE(result.has_value());

    auto& mod = result.value();
    EXPECT_TRUE(mod.base().valid());
    EXPECT_GT(mod.size(), 0u);
}

TEST(Module, GetExportLoadLibraryA) {
    auto mod_result = Module::from_name("kernel32.dll");
    ASSERT_TRUE(mod_result.has_value());

    auto& mod = mod_result.value();
    auto export_result = mod.get_export("LoadLibraryA");
    ASSERT_TRUE(export_result.has_value());
    EXPECT_TRUE(export_result.value().valid());
}
