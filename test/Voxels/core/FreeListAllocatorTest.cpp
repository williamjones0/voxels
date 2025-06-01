#include "gtest/gtest.h"

#include "Voxels/core/FreeListAllocator.hpp"

TEST(FreeListAllocatorTest, AllocateAndDeallocate) {
    FreeListAllocator allocator(1024, 16);

    auto region = allocator.allocate(32);
    ASSERT_TRUE(region.has_value());
    EXPECT_EQ(region->offset, 0);
    EXPECT_EQ(region->length, 32);

    allocator.deallocate(region->offset, region->length);
    auto newRegion = allocator.allocate(32);
    ASSERT_TRUE(newRegion.has_value());
    EXPECT_EQ(newRegion->offset, 0);
    EXPECT_EQ(newRegion->length, 32);
}
