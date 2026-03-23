cmake_minimum_required(VERSION 3.10)
project(capital_viewer C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Raylib settings
set(RAYLIB_VERSION 5.5)
set(RAYLIB_URL "https://github.com/raysan5/raylib.git")

# Find raylib
find_package(raylib ${RAYLIB_VERSION} QUIET)
if(NOT raylib_FOUND)
    # Fetch raylib if not found
    include(FetchContent)
    FetchContent_Declare(
        raylib
        GIT_REPOSITORY ${RAYLIB_URL}
        GIT_TAG ${RAYLIB_VERSION}
    )
    FetchContent_MakeAvailable(raylib)
endif()

# Executable
add_executable(capital_viewer
    main.c
    ui.c
    archive.c
)

target_link_libraries(capital_viewer raylib m)
target_include_directories(capital_viewer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include)

# Disable warnings for external headers
if(MSVC)
    target_compile_options(capital_viewer PRIVATE /W0)
else()
    target_compile_options(capital_viewer PRIVATE -w)
endif()
