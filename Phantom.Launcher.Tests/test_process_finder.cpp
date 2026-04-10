#include <gtest/gtest.h>
#include "launcher/process_finder.h"

using namespace phantom::launcher;

TEST(ProcessFinderTest, FindNonExistentProcessFails)
{
    auto result = ProcessFinder::find_by_name("nonexistent_process_xyz_12345.exe");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), FindError::ProcessNotFound);
}

TEST(ProcessFinderTest, FindExplorerSucceeds)
{
    auto result = ProcessFinder::find_by_name("explorer.exe");
    ASSERT_TRUE(result.has_value()) << "explorer.exe should be running on Windows";
    EXPECT_GT(result.value(), 0u);
}
