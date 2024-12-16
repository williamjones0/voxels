#include "VoxelsApplication.hpp"

int main() {
    VoxelsApplication app;
    app.run();

    return 0;
}

//#include "core/FreeListAllocator.hpp"
//
//int main() {
//    FreeListAllocator allocator(1024, 16);
//
//    auto chunk1 = allocator.allocate(100);
//    if (chunk1) {
//        std::cout << "Allocated Chunk 1: Offset = " << chunk1->offset
//                  << ", Length = " << chunk1->length << '\n';
//    }
//
//    auto chunk2 = allocator.allocate(200);
//    if (chunk2) {
//        std::cout << "Allocated Chunk 2: Offset = " << chunk2->offset
//                  << ", Length = " << chunk2->length << '\n';
//    }
//
//    allocator.printFreeRegions();
//
//    if (chunk1) {
//        allocator.deallocate(*chunk1);
//    }
//    std::cout << "After deallocating Chunk 1:\n";
//    allocator.printFreeRegions();
//
//    auto chunk3 = allocator.allocate(150);
//    if (chunk3) {
//        std::cout << "Allocated Chunk 3: Offset = " << chunk3->offset
//                  << ", Length = " << chunk3->length << '\n';
//    }
//
//    allocator.printFreeRegions();
//
//    return 0;
//}
