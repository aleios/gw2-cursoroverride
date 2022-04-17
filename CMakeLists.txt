cmake_minimum_required(VERSION 3.21)

add_subdirectory(externals/minhook)

project(curoverride)

# Collect ImGui sources
FILE(GLOB IMGUI_SOURCES externals/imgui/*.cpp externals/imgui/misc/cpp/*.cpp externals/imgui/*.c)

# Create shared library target
add_library(curoverride SHARED)
target_sources(curoverride PUBLIC main.cpp ${IMGUI_SOURCES})


set_target_properties(curoverride PROPERTIES CXX_STANDARD 20)

# Link in minhook
target_link_libraries(curoverride minhook)

target_compile_definitions(curoverride PUBLIC "-DNOMINMAX")

target_include_directories(curoverride PUBLIC
     "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/externals/imgui>"
     "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/externals/minhook/include>"
)