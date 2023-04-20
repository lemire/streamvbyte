cmake_minimum_required(VERSION 3.3)

set(CMAKE_MACOSX_RPATH OFF)
project(streamvbyte VERSION "0.5.3" LANGUAGES C)

set(STREAMVBYTE_LIB_VERSION "0.5.3" CACHE STRING "streamvbyte library version")
set(STREAMVBYTE_LIB_SOVERSION "1" CACHE STRING "streamvbyte library soversion")

# Create target
add_library(
    streamvbyte
    src/streamvbyte_encode.c
    src/streamvbyte_decode.c
    src/streamvbyte_zigzag.c
    src/streamvbytedelta_encode.c
    src/streamvbytedelta_decode.c
    src/streamvbyte_0124_encode.c
    src/streamvbyte_0124_decode.c
)

# Add alias
add_library(streamvbyte::streamvbyte ALIAS streamvbyte)

# Set C standard
target_compile_features(streamvbyte PUBLIC c_std_99)
set_target_properties(streamvbyte PROPERTIES C_STANDARD_REQUIRED ON)

# Set fPIC
set_target_properties(streamvbyte PROPERTIES POSITION_INDEPENDENT_CODE ON)

option(STREAMVBYTE_SANITIZE "Sanitize addresses" OFF)

# Set CMAKE_BUILD_TYPE
if(NOT CMAKE_BUILD_TYPE)
    message(STATUS "No build type selected")
    if(STREAMVBYTE_SANITIZE)
        message(STATUS "Default to Debug")
        set(CMAKE_BUILD_TYPE Debug
            CACHE STRING "Choose the type of build."
            FORCE
        )
    else()
        message(STATUS "Default to Release")
        set(CMAKE_BUILD_TYPE Release
            CACHE STRING "Choose the type of build."
            FORCE
        )
    endif()
endif()

if(STREAMVBYTE_SANITIZE)
    message(STATUS "Enabling memory sanitizer.")
    add_compile_options(
        -fsanitize=address
        -fno-omit-frame-pointer
        -fno-sanitize-recover=all
    )
    add_compile_definitions(ASAN_OPTIONS=detect_leaks=1)
endif()

option(STREAMVBYTE_SANITIZE_UNDEFINED "Sanitize undefined behavior" OFF)
if(STREAMVBYTE_SANITIZE_UNDEFINED)
    add_compile_options(-fsanitize=undefined -fno-sanitize-recover=all)
    add_link_options(-fsanitize=undefined -fno-sanitize-recover=all)
endif()

if(MSVC)
    target_compiler_definitions(streamvbyte PRIVATE "__restrict__=__restrict")
endif()

# test for arm
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64.*|AARCH64.*)")
    set(BASE_FLAGS ${BASE_FLAGS} "-D__ARM_NEON__")
endif()

target_link_libraries(streamvbyte ${BASE_FLAGS})

# Set version
set_target_properties(
    streamvbyte
    PROPERTIES
        VERSION "${STREAMVBYTE_LIB_VERSION}"
        SOVERSION "${STREAMVBYTE_LIB_SOVERSION}"
        WINDOWS_EXPORT_ALL_SYMBOLS YES
)

# Add root include directory
target_include_directories(
    streamvbyte
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/include>
)

# Print info
message(STATUS "CMAKE_SYSTEM_PROCESSOR: " ${CMAKE_SYSTEM_PROCESSOR})
message(STATUS "CMAKE_BUILD_TYPE: " ${CMAKE_BUILD_TYPE}) # this tends to be "sticky" so you can remain unknowingly in debug mode
message(STATUS "CMAKE_C_COMPILER: " ${CMAKE_C_COMPILER}) # important to know which compiler is used
message(STATUS "CMAKE_C_FLAGS: " ${CMAKE_C_FLAGS}) # important to know the flags
message(STATUS "CMAKE_C_FLAGS_DEBUG: " ${CMAKE_C_FLAGS_DEBUG})
message(STATUS "CMAKE_C_FLAGS_RELEASE: " ${CMAKE_C_FLAGS_RELEASE})

# Example
option(STREAMVBYTE_ENABLE_EXAMPLES "Enable examples for streamvbyte" OFF)
if(STREAMVBYTE_ENABLE_EXAMPLES)
    add_executable(example examples/example.c)
    target_link_libraries(example streamvbyte)
endif()

# Tests
option(STREAMVBYTE_ENABLE_TESTS "Enable unit tests for streamvbyte" OFF)
if(STREAMVBYTE_ENABLE_TESTS)
    if(NOT MSVC)
        # Perf
        add_executable(perf tests/perf.c)
        target_link_libraries(perf streamvbyte)
        target_link_libraries(perf m)
    endif()

    # Writeseq
    add_executable(writeseq tests/writeseq.c)
    target_link_libraries(writeseq streamvbyte)

    # Unit
    add_executable(unit tests/unit.c)
    target_link_libraries(unit streamvbyte)

    enable_testing()

    # Add unit tests
    add_test(NAME unit COMMAND unit)
    add_custom_target(check COMMAND ctest --output-on-failure DEPENDS unit)
endif()

# Install
include(GNUInstallDirs)

install(TARGETS streamvbyte EXPORT streamvbyte-target)
install(
    EXPORT streamvbyte-target
    FILE streamvbyte-targets.cmake
    NAMESPACE streamvbyte::
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/${PROJECT_NAME}/cmake
)

include(CMakePackageConfigHelpers)

# Create package config file
configure_package_config_file(
    "${PROJECT_SOURCE_DIR}/cmake/streamvbyte-config.cmake.in"
    "streamvbyte-config.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/${PROJECT_NAME}/cmake
)

# Create package version file
write_basic_package_version_file(
    streamvbyte-config-version.cmake
    COMPATIBILITY SameMajorVersion
)

# Install cmake files
install(
    FILES
        "${PROJECT_BINARY_DIR}/streamvbyte-config.cmake"
        "${PROJECT_BINARY_DIR}/streamvbyte-config-version.cmake"
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/${PROJECT_NAME}/cmake
)

install(
    FILES
        ${PROJECT_SOURCE_DIR}/include/streamvbyte.h
        ${PROJECT_SOURCE_DIR}/include/streamvbytedelta.h
        ${PROJECT_SOURCE_DIR}/include/streamvbyte_zigzag.h
    DESTINATION include
)
