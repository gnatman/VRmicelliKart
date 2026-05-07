cmake_minimum_required(VERSION 3.16.0 FATAL_ERROR)

# Set the project version and language
project(Spaghettify VERSION 1.0.0 LANGUAGES C CXX ASM)
include(FetchContent)

set(NATO_PHONETIC_ALPHABET
  "Alfa" "Bravo" "Charlie" "Delta" "Echo" "Foxtrot" "Golf" "Hotel"
  "India" "Juliett" "Kilo" "Lima" "Mike" "November" "Oscar" "Papa"
  "Quebec" "Romeo" "Sierra" "Tango" "Uniform" "Victor" "Whiskey"
  "Xray" "Yankee" "Zulu"
)

# Get the patch version number from the project version
math(EXPR PATCH_INDEX "${PROJECT_VERSION_PATCH}")

# Use the patch number to select the correct word
list(GET NATO_PHONETIC_ALPHABET ${PATCH_INDEX} PROJECT_PATCH_WORD)

set(PROJECT_BUILD_NAME "Bolognese ${PROJECT_PATCH_WORD}" CACHE STRING "" FORCE)
set(PROJECT_TEAM "SpaghettiKart Team - HarbourMasters")

execute_process(
  COMMAND git config --global --add safe.directory ${CMAKE_SOURCE_DIR}
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_QUIET
)

execute_process(
  COMMAND git describe --tags
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE PROJECT_VERSION
  OUTPUT_STRIP_TRAILING_WHITESPACE
)
if (NOT DEFINED PROJECT_VERSION OR "${PROJECT_VERSION}" STREQUAL "")
  set(PROJECT_VERSION "1.0.0")
endif()

message("Spaghetti Kart version: ${PROJECT_VERSION} ${PROJECT_PATCH_WORD}")

if(APPLE)
  enable_language(OBJCXX)
endif()

# Global definitions using add_definitions to ensure they propagate
add_definitions(
    -DVERSION_US=1
    -DENABLE_RUMBLE=1
    -DF3DEX_GBI=1
    -D_LANGUAGE_C
    -D_USE_MATH_DEFINES
    -DNOMINMAX
    -DCIMGUI_DEFINE_ENUMS_AND_STRUCTS
    -DNON_MATCHING=1
    -DNON_EQUIVALENT=1
    -DAVOID_UB=1
    -DSPAGHETTI_VERSION="${PROJECT_VERSION}"
)

# Set the C++ standard and enable the MSVC parallel build option
set(CMAKE_CXX_STANDARD 20 CACHE STRING "The C++ standard to use")
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11 CACHE STRING "The C standard to use")

set_property(DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT Spaghettify)

# Add a custom module path to locate additional CMake modules
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/modules/")
set(YAML_CPP_STATIC_DEFINE ON)
add_compile_definitions(YAML_CPP_STATIC_DEFINE)
include(cmake/lus-cvars.cmake)

if (WIN32)
  include(cmake/automate-vcpkg.cmake)
  set(VCPKG_TRIPLET x64-windows-static)
  set(VCPKG_TARGET_TRIPLET x64-windows-static)
  vcpkg_bootstrap()
  vcpkg_install_packages()

  set_property(DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})
  set_property(DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()

if (MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# Collection of source files
file(GLOB ALL_FILES_ROOT RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    "src/*.h"
    "src/*.cpp"
    "src/os/*.cpp"
    "src/os/*.h"
)

file(GLOB_RECURSE ALL_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    "src/buffers/*.h"
    "src/buffers/*.cpp"
    "src/data/*.h"
    "src/data/*.cpp"
    "src/debug/*.h"
    "src/debug/*.cpp"
    "src/ending/*.h"
    "src/ending/*.cpp"
    "src/racing/*.h"
    "src/racing/*.cpp"
    "src/audio/*.h"
    "src/audio/*.cpp"
    "src/port/*.h"
    "src/port/*.cpp"
    "src/mods/*.h"
    "src/mods/*.cpp"
    "src/engine/*.cpp"
    "src/engine/*.h"
    "src/engine/tracks/*.cpp"
    "src/engine/tracks/*.h"
    "src/engine/actors/*.cpp"
    "src/engine/actors/*.h"
    "src/engine/objects/*.cpp"
    "src/engine/objects/*.h"
    "src/engine/sky/*.cpp"
    "src/engine/sky/*.h"
    "src/engine/particles/*.cpp"
    "src/engine/particles/*.h"
    "src/enhancements/*.cpp"
    "src/enhancements/*.h"
    "src/enhancements/freecam/*.cpp"
    "src/enhancements/freecam/*.h"
    "src/enhancements/telemetry/*.cpp"
    "src/enhancements/telemetry/*.h"
    "src/enhancements/vr/*.cpp"
    "src/enhancements/vr/*.h"
    "src/enhancements/wheel/*.cpp"
    "src/enhancements/wheel/*.h"
    "Resource.rc"
)

# Force include VR and FFB files from LUS into the main project to ensure they are linked
file(GLOB_RECURSE LUS_EXTRA_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    "libultraship/src/vr/*.cpp"
    "libultraship/src/ship/ffb/*.cpp"
    "libultraship/src/ship/events/*.cpp"
)
list(APPEND ALL_FILES ${LUS_EXTRA_FILES})

list(APPEND ALL_FILES ${ALL_FILES_ROOT})
list(REMOVE_DUPLICATES ALL_FILES)
list(FILTER ALL_FILES EXCLUDE REGEX ".*.inc.cpp")
list(FILTER ALL_FILES EXCLUDE REGEX "./src/debug/crash_screen_enhancement.cpp")
list(FILTER ALL_FILES EXCLUDE REGEX "src/harbour_masters.cpp")

add_executable(${PROJECT_NAME} ${ALL_FILES})

# External deps
find_package(nlohmann_json CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE nlohmann_json::nlohmann_json)

find_package(spdlog CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE spdlog::spdlog)

find_package(SDL2 CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>)

find_package(SDL2_net CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE $<IF:$<TARGET_EXISTS:SDL2_net::SDL2_net>,SDL2_net::SDL2_net,SDL2_net::SDL2_net-static>)

find_package(OpenXR CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE OpenXR::headers $<IF:$<TARGET_EXISTS:OpenXR::openxr_loader>,OpenXR::openxr_loader,OpenXR::openxr_loader-static>)

find_package(Ogg CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE Ogg::ogg)

find_package(Vorbis CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE Vorbis::vorbisfile Vorbis::vorbis Vorbis::vorbisenc)

if (MSVC)
    # Allow legacy string literal to non-const pointer conversion to fix MK64 code in C++
    target_compile_options(${PROJECT_NAME} PRIVATE /Zc:strictStrings- /permissive)
endif()

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/assets
    ${CMAKE_CURRENT_SOURCE_DIR}/include/assets/models
    ${CMAKE_CURRENT_SOURCE_DIR}/include/assets/textures
    ${CMAKE_CURRENT_SOURCE_DIR}/include/assets/other
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/port
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/objects
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/actors
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/tracks
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/sky
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/particles
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/cameras
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/editor
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/registry
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/vehicles
    ${CMAKE_CURRENT_SOURCE_DIR}/src/racing
    ${CMAKE_CURRENT_SOURCE_DIR}/src/audio
    ${CMAKE_CURRENT_SOURCE_DIR}/src/buffers
    ${CMAKE_CURRENT_SOURCE_DIR}/src/data
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ending
    ${CMAKE_CURRENT_SOURCE_DIR}/src/enhancements
    ${CMAKE_CURRENT_SOURCE_DIR}/courses
    ${CMAKE_CURRENT_SOURCE_DIR}/torch/src
    ${CMAKE_CURRENT_SOURCE_DIR}/torch/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/libultraship/include
    ${CMAKE_CURRENT_SOURCE_DIR}/libultraship/include/libultraship
    ${CMAKE_CURRENT_SOURCE_DIR}/libultraship/src
    ${CMAKE_BINARY_DIR}/_deps/tomlplusplus-src/include
    ${CMAKE_BINARY_DIR}/_deps/yaml-cpp-src/include
)

# Libultraship
add_subdirectory(libultraship ${CMAKE_CURRENT_SOURCE_DIR}/libultraship)
target_link_libraries(${PROJECT_NAME} PRIVATE libultraship)

# Torch
add_subdirectory(torch)

if (WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32)
endif()

# Asset extraction
add_custom_target(ExtractAssets
    COMMAND ${PYTHON_EXECUTABLE} extract_assets.py us
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

# Packaging
set(CPACK_PACKAGE_NAME "Spaghettify")
set(CPACK_PACKAGE_VENDOR "Harbour Masters")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
if(WIN32)
  set(CPACK_GENERATOR "ZIP")
endif()
include(CPack)
