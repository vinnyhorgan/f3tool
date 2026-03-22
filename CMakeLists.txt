cmake_minimum_required(VERSION 3.16)
project(f3tool C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(BUILD_TOOLS "Build command line tools" ON)
option(BUILD_LIB "Build shared library" OFF)

# Core libraries we need
find_package(PkgConfig REQUIRED)

# zlib for BSA decompression
pkg_check_modules(ZLIB REQUIRED zlib)

# libogg for OGG container
pkg_check_modules(OGG REQUIRED ogg)

# libvorbis for OGG encoding  
pkg_check_modules(VORBIS REQUIRED vorbis)
pkg_check_modules(VORBISENC REQUIRED vorbisenc)
pkg_check_modules(VORBISFILE REQUIRED vorbisfile)

# libpng for PNG output
pkg_check_modules(PNG REQUIRED libpng)

# libjpeg for DDS decompression (if needed)
pkg_check_modules(JPEG REQUIRED libjpeg)

include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${ZLIB_INCLUDE_DIRS}
    ${OGG_INCLUDE_DIRS}
    ${VORBIS_INCLUDE_DIRS}
    ${VORBISENC_INCLUDE_DIRS}
    ${VORBISFILE_INCLUDE_DIRS}
    ${PNG_INCLUDE_DIRS}
    ${JPEG_INCLUDE_DIRS}
)

set(SOURCES
    src/main.c
    src/bsa.c
    src/esm.c
    src/archive.c
    src/audio.c
    src/image.c
    src/hash.c
    src/util.c
)

set(HEADERS
    include/bsa.h
    include/esm.h
    include/archive.h
    include/audio.h
    include/image.h
    include/hash.h
    include/util.h
)

if(BUILD_TOOLS)
    add_executable(f3tool ${SOURCES} ${HEADERS})
    target_link_libraries(f3tool
        ${ZLIB_LIBRARIES}
        ${OGG_LIBRARIES}
        ${VORBIS_LIBRARIES}
        ${VORBISENC_LIBRARIES}
        ${VORBISFILE_LIBRARIES}
        ${PNG_LIBRARIES}
        ${JPEG_LIBRARIES}
        m
    )
endif()

if(BUILD_LIB)
    add_library(f3tool_lib SHARED ${SOURCES} ${HEADERS})
    target_link_libraries(f3tool_lib
        ${ZLIB_LIBRARIES}
        ${OGG_LIBRARIES}
        ${VORBIS_LIBRARIES}
        ${VORBISENC_LIBRARIES}
        ${VORBISFILE_LIBRARIES}
        ${PNG_LIBRARIES}
        ${JPEG_LIBRARIES}
        m
    )
endif()
