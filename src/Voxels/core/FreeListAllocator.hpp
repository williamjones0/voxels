#pragma once

#include <iostream>
#include <list>
#include <optional>
#include <algorithm>

struct Region {
    size_t offset; // Start offset of the region
    size_t length; // Length of the region

    bool operator<(const Region &other) const {
        return offset < other.offset;
    }
};

class FreeListAllocator {
public:
    FreeListAllocator() = default;

    FreeListAllocator(size_t bufferSize, size_t alignment)
            : bufferSize(bufferSize), alignment(alignment) {
        // Initially, the whole buffer is a single free region
        freeRegions.push_back({0, bufferSize});
    }

    std::optional<Region> allocate(size_t vertexCount) {
        size_t alignedSize = align(vertexCount, alignment); // Round up size to alignment
        for (auto it = freeRegions.begin(); it != freeRegions.end(); ++it) {
            // Check if this region can fit the aligned allocation
            if (it->length >= alignedSize) {
                // Allocate the entire aligned block
                size_t regionStart = it->offset;

                // Adjust the free region
                if (it->length > alignedSize) {
                    it->offset += alignedSize;
                    it->length -= alignedSize;
                } else {
                    freeRegions.erase(it); // Remove the free region if fully consumed
                }

                // Return the allocated region
                return Region{regionStart, alignedSize};
            }
        }

        // If no suitable free region found, return std::nullopt
        return std::nullopt;
    }

    void deallocate(size_t offset, size_t length) {
        length = align(length, alignment); // Round up size to alignment
        Region region{offset, length};

        // Insert the region in the correct position to maintain sorted order
        auto it = std::lower_bound(freeRegions.begin(), freeRegions.end(), region);
        freeRegions.insert(it, region);

        // Merge adjacent regions
        mergeFreeRegions();
    }

    void printFreeRegions() const {
        std::cout << "Free Regions:\n";
        for (const auto &region : freeRegions) {
            std::cout << "Offset: " << region.offset << ", Length: " << region.length << '\n';
        }
    }

private:
    size_t bufferSize{};
    size_t alignment{};
    std::list<Region> freeRegions;

    static size_t align(size_t offset, size_t alignment) {
        return (offset + alignment - 1) & ~(alignment - 1);
    }

    void mergeFreeRegions() {
        for (auto it = freeRegions.begin(); it != freeRegions.end();) {
            auto next = std::next(it);
            if (next != freeRegions.end() && it->offset + it->length == next->offset) {
                // Merge current and next regions
                it->length += next->length;
                freeRegions.erase(next);
            } else {
                ++it;
            }
        }
    }
};
