# Native engine for the Freeking Android port.
# Built by Gradle (externalNativeBuild) as libfreeking.so, loaded by
# GameActivity together with libSDL2.so. GLES3 + SDL audio, no OpenAL.

cmake_minimum_required(VERSION 3.22)

project(freeking CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- SDL2 (sources downloaded by the fetchSdl Gradle task) ---
if(NOT DEFINED SDL_SRC_DIR OR NOT EXISTS "${SDL_SRC_DIR}/CMakeLists.txt")
    # Fallback for manual ndk-build style setups: fetch a pinned release.
    include(FetchContent)
    FetchContent_Declare(SDL2
        URL "https://github.com/libsdl-org/SDL/archive/refs/tags/release-2.30.0.tar.gz")
    FetchContent_MakeAvailable(SDL2)
    set(SDL_SRC_DIR "${sdl2_SOURCE_DIR}")
else()
    add_subdirectory("${SDL_SRC_DIR}" SDL2-build)
endif()

# --- Engine sources (mirror of Source/CMakeLists.txt, minus desktop-only bits) ---
set(FREEKING_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../Source")

file(GLOB_RECURSE FREEKING_SOURCES
    "${FREEKING_SOURCE_DIR}/*.cpp"
    "${FREEKING_SOURCE_DIR}/*.c")

# Desktop GL loaders are unused on GLES (GLCompat.h uses the NDK headers).
list(FILTER FREEKING_SOURCES EXCLUDE REGEX ".*/ThirdParty/glad/.*")

add_library(freeking SHARED ${FREEKING_SOURCES})

target_include_directories(freeking PRIVATE
    "${FREEKING_SOURCE_DIR}/ThirdParty/stb"
    "${FREEKING_SOURCE_DIR}/ThirdParty/json"
    "${FREEKING_SOURCE_DIR}/Bsp"
    "${FREEKING_SOURCE_DIR}/Color"
    "${FREEKING_SOURCE_DIR}/Core"
    "${FREEKING_SOURCE_DIR}/Entities"
    "${FREEKING_SOURCE_DIR}/FileSystems"
    "${FREEKING_SOURCE_DIR}/Game"
    "${FREEKING_SOURCE_DIR}/Input"
    "${FREEKING_SOURCE_DIR}/Loaders"
    "${FREEKING_SOURCE_DIR}/Math"
    "${FREEKING_SOURCE_DIR}/Md2"
    "${FREEKING_SOURCE_DIR}/Mdx"
    "${FREEKING_SOURCE_DIR}/Nav"
    "${FREEKING_SOURCE_DIR}/Render")

# Keep default symbol visibility: SDL2.so resolves our SDL_main at runtime.
target_compile_options(freeking PRIVATE
    -ffunction-sections
    -fdata-sections
    -fno-strict-aliasing
    -Wall)

target_link_options(freeking PRIVATE
    "LINKER:--gc-sections,--icf=all")

target_link_libraries(freeking PRIVATE
    SDL2::SDL2
    GLESv3
    EGL
    log
    android)
