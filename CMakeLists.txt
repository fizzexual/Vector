# Vector — CEF-based desktop game launcher (Windows x64).
cmake_minimum_required(VERSION 3.21)

# CEF only supports Debug and Release configuration types.
set(CMAKE_CONFIGURATION_TYPES Debug Release)

project(vector CXX C)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Build a plain wWinMain executable (no sandbox bootstrap DLL).
option(USE_SANDBOX "Enable or disable use of the sandbox." OFF)

# Locate the bundled CEF binary distribution.
set(CEF_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/cef")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CEF_ROOT}/cmake")
find_package(CEF REQUIRED)

# Build the CEF DLL wrapper static library.
add_subdirectory(${CEF_LIBCEF_DLL_WRAPPER_PATH} libcef_dll_wrapper)

# ---------------------------------------------------------------------------
# Vector application target
# ---------------------------------------------------------------------------
set(VECTOR_TARGET "Vector")

file(GLOB_RECURSE VECTOR_SRCS CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h")

# Logical target used to link the libcef library.
ADD_LOGICAL_TARGET("libcef_lib" "${CEF_LIB_DEBUG}" "${CEF_LIB_RELEASE}")

SET_CEF_TARGET_OUT_DIR()

add_executable(${VECTOR_TARGET} WIN32 ${VECTOR_SRCS})
SET_EXECUTABLE_TARGET_PROPERTIES(${VECTOR_TARGET})
add_dependencies(${VECTOR_TARGET} libcef_dll_wrapper)
target_link_libraries(${VECTOR_TARGET} libcef_lib libcef_dll_wrapper ${CEF_STANDARD_LIBS})

# Our own headers (so "core/...", "app/...", "bridge/...", "third_party/..." resolve).
target_include_directories(${VECTOR_TARGET} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")

# CEF 149 requires C++20.
set_target_properties(${VECTOR_TARGET} PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)

# Embed the Windows manifests (DPI awareness / OS version compatibility).
ADD_WINDOWS_MANIFEST("${CMAKE_CURRENT_SOURCE_DIR}/win" "${VECTOR_TARGET}" "exe")

# Copy CEF binaries and resources next to the executable.
COPY_FILES("${VECTOR_TARGET}" "${CEF_BINARY_FILES}" "${CEF_BINARY_DIR}" "${CEF_TARGET_OUT_DIR}")
COPY_FILES("${VECTOR_TARGET}" "${CEF_RESOURCE_FILES}" "${CEF_RESOURCE_DIR}" "${CEF_TARGET_OUT_DIR}")

# Copy the renderer UI (HTML/CSS/JS) next to the executable.
add_custom_command(TARGET ${VECTOR_TARGET} POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/app"
    "${CEF_TARGET_OUT_DIR}/resources/app"
  VERBATIM)

PRINT_CEF_CONFIG()
