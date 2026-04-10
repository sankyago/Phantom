#include <gtest/gtest.h>
#include "launcher/registry_helper.h"

using namespace phantom::launcher;

TEST(RegistryHelperTest, NonExistentKeyReturnsError)
{
    auto result = RegistryHelper::read_string(
        HKEY_LOCAL_MACHINE,
        R"(SOFTWARE\PhantomNonExistentKey_12345)",
        "SomeValue");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), RegistryError::KeyNotFound);
}

TEST(RegistryHelperTest, SystemRootExists)
{
    auto result = RegistryHelper::read_string(
        HKEY_LOCAL_MACHINE,
        R"(SOFTWARE\Microsoft\Windows NT\CurrentVersion)",
        "SystemRoot");
    ASSERT_TRUE(result.has_value()) << "SystemRoot registry value should exist on Windows";
    EXPECT_FALSE(result.value().empty());
}
