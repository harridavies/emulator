cmake_minimum_required(VERSION 3.20)
project(Emulator LANGUAGES CXX)

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_CURRENT_BINARY_DIR)
    message(FATAL_ERROR "Use an out-of-source build: cmake -S . -B build")
endif()

include(CTest)

# CONFIGURE_DEPENDS makes subsequent builds discover added/removed files.
# Exclude build trees so generated compiler-check sources are never compiled.
file(GLOB_RECURSE VM_FILES CONFIGURE_DEPENDS
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cxx"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hh"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hxx"
)
file(RELATIVE_PATH VM_BUILD_DIRECTORY
    "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}")

set(VM_SOURCES)
set(VM_HEADERS)
set(VM_TEST_SOURCES)
foreach(file IN LISTS VM_FILES)
    string(FIND "${file}" "${VM_BUILD_DIRECTORY}/" in_build_directory)
    if(in_build_directory EQUAL 0 OR
       file MATCHES "(^|/)(build[^/]*|cmake-build[^/]*|CMakeFiles|_deps|\\.git)(/|$)")
        continue()
    endif()

    # Each test source is a standalone executable with its own main().
    # Tests may live under test/ or tests/, or use test_*, *_test, *_tests.
    if(file MATCHES "(^|/)(test|tests)/" OR
       file MATCHES "(^|/)(test|test_[^/]*|[^/]*_tests?)\\.(cpp|cc|cxx)$")
        if(file MATCHES "\\.(cpp|cc|cxx)$")
            list(APPEND VM_TEST_SOURCES "${file}")
        endif()
    elseif(file MATCHES "\\.(h|hh|hpp|hxx)$")
        list(APPEND VM_HEADERS "${file}")
    elseif(NOT file STREQUAL "main.cpp")
        list(APPEND VM_SOURCES "${file}")
    endif()
endforeach()

# Share production code with tests without linking the application's main().
# The current implementation is header-only; added .cpp files automatically
# turn this into a compiled library on the next build.
if(VM_SOURCES)
    add_library(vm_core STATIC ${VM_SOURCES} ${VM_HEADERS})
    set(VM_CORE_SCOPE PUBLIC)
else()
    add_library(vm_core INTERFACE)
    target_sources(vm_core INTERFACE ${VM_HEADERS})
    set(VM_CORE_SCOPE INTERFACE)
endif()
target_compile_features(vm_core ${VM_CORE_SCOPE} cxx_std_20)
set_target_properties(vm_core PROPERTIES CXX_EXTENSIONS OFF)
target_include_directories(vm_core ${VM_CORE_SCOPE}
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

add_executable(vm main.cpp)
target_link_libraries(vm PRIVATE vm_core)
set_target_properties(vm PROPERTIES CXX_EXTENSIONS OFF)

if(BUILD_TESTING)
    foreach(test_source IN LISTS VM_TEST_SOURCES)
        # Include a path hash so equal filenames in different folders are safe.
        get_filename_component(test_name "${test_source}" NAME_WE)
        string(SHA256 test_hash "${test_source}")
        string(SUBSTRING "${test_hash}" 0 12 test_hash)
        set(test_target "vm_test_${test_name}_${test_hash}")
        add_executable("${test_target}" "${test_source}")
        target_link_libraries("${test_target}" PRIVATE vm_core)
        set_target_properties("${test_target}" PROPERTIES CXX_EXTENSIONS OFF)
        add_test(NAME "${test_source}" COMMAND "${test_target}")
        set_tests_properties("${test_source}" PROPERTIES
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
    endforeach()
endif()
