cmake_minimum_required(VERSION 3.10)

# Project name
project(VulkanLearning)

# C++ Standard
set(CMAKE_CXX_STANDARD 20)

# Set SDL_STATIC to ON to build SDL3 as a static library 
set(SDL_STATIC ON)

# Set all source files
set(SOURCE_FILES
        src/main.cpp
        src/logger.cpp
        src/vulkan_base/vulkan_device.cpp
        src/vulkan_base/vulkan_swapchain.cpp
        src/vulkan_base/vulkan_renderpass.cpp
        src/vulkan_base/vulkan_pipeline.cpp
        src/vulkan_base/vulkan_utils.cpp
        src/model.cpp
)

# Find SDL2
add_subdirectory(libs/SDL)

# Find Vulkan
find_package(Vulkan REQUIRED)

# Add executable target with source files listed in SOURCE_FILES variable
add_executable(VulkanLearning ${SOURCE_FILES})

# Add /src to search path for headers
target_include_directories(VulkanLearning PUBLIC ${PROJECT_SOURCE_DIR}/src)

# Include SDL3
target_include_directories(VulkanLearning PUBLIC libs/SDL/include)
target_link_libraries(VulkanLearning PUBLIC SDL3-static)

# Include Vulkan
target_include_directories(VulkanLearning PUBLIC ${Vulkan_INCLUDE_DIRS})
target_link_libraries(VulkanLearning PUBLIC ${Vulkan_LIBRARIES})

# Include stb
target_include_directories(VulkanLearning PUBLIC libs/stb)

# Include cgltf
target_include_directories(VulkanLearning PUBLIC libs/cgltf)

# Shader compilation
include("CMake/CompileShaders.cmake")

set(SHADERS
        "${CMAKE_SOURCE_DIR}/src/shaders/triangle.vert"
        "${CMAKE_SOURCE_DIR}/src/shaders/triangle.frag"
        "${CMAKE_SOURCE_DIR}/src/shaders/color.vert"
        "${CMAKE_SOURCE_DIR}/src/shaders/color.frag"
        "${CMAKE_SOURCE_DIR}/src/shaders/texture.vert"
        "${CMAKE_SOURCE_DIR}/src/shaders/texture.frag"
        "${CMAKE_SOURCE_DIR}/src/shaders/model.vert"
        "${CMAKE_SOURCE_DIR}/src/shaders/model.frag"
)

compile_shaders(shaders "${CMAKE_SOURCE_DIR}/src/shaders" "${CMAKE_BINARY_DIR}/shaders" ${SHADERS})
add_dependencies(VulkanLearning shaders)

# Copy data
include("CMake/CopyDirectory.cmake")

add_copy_directory(data_dir "${CMAKE_SOURCE_DIR}/data" "${CMAKE_BINARY_DIR}/data")
add_dependencies(VulkanLearning data_dir)

# Set build specific macro
target_compile_definitions(VulkanLearning PRIVATE
        $<$<CONFIG:Debug>:DEBUG_BUILD>
        $<$<CONFIG:Release>:RELEASE_BUILD>
)
