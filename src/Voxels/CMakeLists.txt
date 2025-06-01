if(MSVC)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
    foreach(OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES})
        string(TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG)
        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_CURRENT_BINARY_DIR})
        set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_CURRENT_BINARY_DIR})
        set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_CURRENT_BINARY_DIR})
    endforeach(OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES})
endif()

add_custom_target(copy_data ALL COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/data ${CMAKE_CURRENT_BINARY_DIR}/data)   

add_library(VoxelsLib
    core/Camera.cpp
    util/Util.cpp
    world/Chunk.cpp
    world/Mesher.cpp
    Application.cpp
    Application.hpp
    VoxelsApplication.hpp
    VoxelsApplication.cpp
    core/ThreadPool.cpp
    core/FreeListAllocator.hpp
    player/Q3PlayerController.cpp
    player/Q3PlayerController.hpp
    io/Input.cpp
    io/Input.hpp
    player/CharacterController.hpp
    player/CharacterController.cpp
    world/WorldManager.cpp
    world/WorldManager.hpp
    core/Transform.hpp
    player/Q1PlayerController.hpp
    player/Q1PlayerController.cpp
    player/FlyPlayerController.hpp
    player/FlyPlayerController.cpp
    world/Level.hpp
    world/Level.cpp
)

target_include_directories(VoxelsLib PUBLIC include)

target_link_libraries(VoxelsLib
    PUBLIC glad glfw imgui glm cgltf stb_image spdlog TracyClient
)

add_executable(Voxels main.cpp)
target_link_libraries(Voxels PRIVATE VoxelsLib)

add_dependencies(Voxels copy_data)
