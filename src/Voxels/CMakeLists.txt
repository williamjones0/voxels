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

file(GLOB_RECURSE VOXELS_SOURCES
        CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/*.hpp
)

add_library(VoxelsLib)
target_sources(VoxelsLib PRIVATE ${VOXELS_SOURCES})

target_include_directories(VoxelsLib PUBLIC include)

target_link_libraries(VoxelsLib
    PUBLIC glad glfw imgui glm cgltf stb_image spdlog TracyClient nlohmann_json::nlohmann_json
)

add_executable(Voxels main.cpp)
target_link_libraries(Voxels PRIVATE VoxelsLib)

add_dependencies(Voxels copy_data)
add_definitions(-DPROJECT_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
