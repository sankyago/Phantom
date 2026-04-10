#include <gtest/gtest.h>

#include "hook/vmt_hook.h"

using namespace phantom::hook;

// ===========================================================================
// Test helpers — fake COM-style interface with a vtable
// ===========================================================================

class FakeInterface {
public:
    virtual ~FakeInterface() = default;
    virtual int method_a() { return 1; }
};

static int hooked_method_a(FakeInterface* /*self*/) {
    return 42;
}

// ===========================================================================
// VMTHook tests
// ===========================================================================

TEST(VMTHook, HookReplacesVirtualMethod) {
    FakeInterface obj;
    EXPECT_EQ(obj.method_a(), 1);

    {
        // Index 1: index 0 is the destructor in the MSVC vtable layout.
        auto result = VMTHook::create(&obj, 1, reinterpret_cast<void*>(&hooked_method_a));
        ASSERT_TRUE(result.has_value());

        EXPECT_EQ(obj.method_a(), 42);
    }

    // Hook destroyed — original should be restored.
    EXPECT_EQ(obj.method_a(), 1);
}

TEST(VMTHook, GetOriginalReturnsOriginalFunction) {
    FakeInterface obj;

    auto result = VMTHook::create(&obj, 1, reinterpret_cast<void*>(&hooked_method_a));
    ASSERT_TRUE(result.has_value());

    EXPECT_NE(result->original(), nullptr);
}

TEST(VMTHook, NullInstanceFails) {
    auto result = VMTHook::create(nullptr, 0, reinterpret_cast<void*>(&hooked_method_a));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), HookError::NullPointer);
}
