
if(MSVC)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
    foreach(OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES})
        string(TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG)
        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_CURRENT_BINARY_DIR})
        set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_CURRENT_BINARY_DIR})
        set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_CURRENT_BINARY_DIR})
    endforeach(OUTPUTCONFIG CMAKE_CONFIGURATION_TYPES)
endif()

add_custom_target(copy_data ALL COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/data ${CMAKE_CURRENT_BINARY_DIR}/data)   

add_executable(Voxels
    core/Camera.cpp
    util/Util.cpp
    world/Chunk.cpp
    world/Mesher.cpp
    world/WorldMesh.cpp
        Application.cpp
        Application.hpp
        main.cpp
        main.cpp
        VoxelsApplication.hpp
        VoxelsApplication.cpp
        core/ThreadPool.cpp
)

add_dependencies(Voxels copy_data)

target_include_directories(Voxels PRIVATE include)

target_link_libraries(Voxels PRIVATE glad glfw imgui glm cgltf stb_image spdlog)
