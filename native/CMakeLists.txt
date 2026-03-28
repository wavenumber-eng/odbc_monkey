# CMakeLists.txt for odbc-monkey
# Zero external dependencies - everything is in the repo
#
# Usage (Visual Studio - recommended):
#   cmake -G "Visual Studio 17 2022" -A x64 -B cmake-build
#   cmake --build cmake-build --config Release
#
# Usage (Ninja with MSVC):
#   cmake -G Ninja -B cmake-build -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl
#   cmake --build cmake-build
#
# Note: MinGW is not supported (wstring/fstream compatibility issues)

cmake_minimum_required(VERSION 3.20)
project(odbc-monkey VERSION 0.1.0 LANGUAGES CXX)

# Version string for filenames (e.g., "a0" for alpha 0)
# Major 0 = alpha (a), 1 = beta (b), 2+ = release
if(PROJECT_VERSION_MAJOR EQUAL 0)
    set(VERSION_LETTER "a")
elseif(PROJECT_VERSION_MAJOR EQUAL 1)
    set(VERSION_LETTER "b")
else()
    set(VERSION_LETTER "v")
endif()
set(VERSION_STRING "${VERSION_LETTER}${PROJECT_VERSION_MINOR}")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Directories
set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(INC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/test)
set(BUILD_DIR ${CMAKE_CURRENT_SOURCE_DIR}/build)

# Generate version header from template
configure_file(
    ${INC_DIR}/version.h.in
    ${BUILD_DIR}/version.h
    @ONLY
)

# Generate version resource from template
configure_file(
    ${SRC_DIR}/version.rc.in
    ${BUILD_DIR}/version.rc
    @ONLY
)

# Source files for the driver DLL
set(DRIVER_SOURCES
    ${SRC_DIR}/odbc_exports.cpp
    ${SRC_DIR}/json_data_source.cpp
    ${SRC_DIR}/file_watcher.cpp
    ${SRC_DIR}/logger.cpp
    ${BUILD_DIR}/version.rc
)

# Main ODBC driver DLL
add_library(odbc-monkey SHARED ${DRIVER_SOURCES})

# Include both original headers and generated version.h
target_include_directories(odbc-monkey PRIVATE ${INC_DIR} ${BUILD_DIR})

# Windows-specific settings
if(WIN32)
    target_compile_definitions(odbc-monkey PRIVATE
        ODBC_MONKEY_EXPORTS
        _UNICODE
        UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _USRDLL
        _WINDLL
    )

    # Link ws2_32 for winsock (logger UDP)
    target_link_libraries(odbc-monkey PRIVATE ws2_32)

    # Versioned DLL name (e.g., odbc-monkey_a1.dll)
    set_target_properties(odbc-monkey PROPERTIES
        OUTPUT_NAME "odbc-monkey_${VERSION_STRING}"
        PREFIX ""
    )

    # MSVC-specific flags
    if(MSVC)
        target_compile_options(odbc-monkey PRIVATE /W3 /EHsc)
    endif()

    # Clang/GCC flags
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(odbc-monkey PRIVATE -Wall -Wextra)
    endif()
endif()

# Test executable
add_executable(odbc_test ${TEST_DIR}/odbc_test.cpp)

target_include_directories(odbc_test PRIVATE ${INC_DIR})

if(WIN32)
    target_compile_definitions(odbc_test PRIVATE
        _UNICODE
        UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )

    target_link_libraries(odbc_test PRIVATE odbc32 user32)

    if(MSVC)
        target_compile_options(odbc_test PRIVATE /W3 /EHsc)
    endif()

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(odbc_test PRIVATE -Wall -Wextra)
    endif()
endif()

# Copy DLL to build/ and bin/ directories
add_custom_command(TARGET odbc-monkey POST_BUILD
    # Copy versioned DLL to build/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:odbc-monkey>
        ${BUILD_DIR}/odbc-monkey_${VERSION_STRING}.dll
    # Also copy as generic name for convenience
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:odbc-monkey>
        ${BUILD_DIR}/odbc-monkey.dll
    # Copy to bin/ for install.ps1
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_SOURCE_DIR}/../bin
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:odbc-monkey>
        ${CMAKE_CURRENT_SOURCE_DIR}/../bin/odbc-monkey_${VERSION_STRING}.dll
    COMMENT "Copying DLL to build/ and bin/"
)

# Install targets
install(TARGETS odbc-monkey
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
)

install(TARGETS odbc_test
    RUNTIME DESTINATION bin
)

# Test target (runs the test executable)
add_custom_target(run_tests
    COMMAND odbc_test
    DEPENDS odbc_test odbc-monkey
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Running ODBC tests"
)
