#include "gtest/gtest.h"

#include "Voxels/core/FreeListAllocator.hpp"

TEST(FreeListAllocatorTest, AllocateAndDeallocate) {
    auto outOfCapacityCallback = [](const size_t size) { return size; };
    FreeListAllocator allocator(1024, 16, outOfCapacityCallback);

    const auto [offset, length] = allocator.allocate(32);
    EXPECT_EQ(offset, 0);
    EXPECT_EQ(length, 32);

    allocator.deallocate(offset, length);
    const auto [newOffset, newLength] = allocator.allocate(32);
    EXPECT_EQ(newOffset, 0);
    EXPECT_EQ(newLength, 32);
}
