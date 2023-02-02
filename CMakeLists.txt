cmake_minimum_required(VERSION 3.3)
set(CMAKE_MACOSX_RPATH OFF)
project(STREAMVBYTE VERSION "1.0.0")

set(STREAMVBYTE_LIB_VERSION "1.0.0" CACHE STRING "streamvbyte library version")
set(STREAMVBYTE_LIB_SOVERSION "1" CACHE STRING "streamvbyte library soversion")

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
option(STREAMVBYTE_SANITIZE "Sanitize addresses" OFF)
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

if(MSVC)
    add_definitions("-D__restrict__=__restrict")
endif()
# test for arm
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64.*|AARCH64.*)")
    set(BASE_FLAGS ${BASE_FLAGS} "-D__ARM_NEON__")
endif()
set(STREAMVBYTE_SRCS
    ${PROJECT_SOURCE_DIR}/src/streamvbyte_encode.c
    ${PROJECT_SOURCE_DIR}/src/streamvbyte_decode.c
    ${PROJECT_SOURCE_DIR}/src/streamvbyte_zigzag.c
    ${PROJECT_SOURCE_DIR}/src/streamvbytedelta_encode.c
    ${PROJECT_SOURCE_DIR}/src/streamvbytedelta_decode.c
    ${PROJECT_SOURCE_DIR}/src/streamvbyte_0124_encode.c
    ${PROJECT_SOURCE_DIR}/src/streamvbyte_0124_decode.c
)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
add_library(streamvbyte_static STATIC "${STREAMVBYTE_SRCS}")
target_link_libraries(streamvbyte_static ${BASE_FLAGS})
add_library(streamvbyte SHARED "${STREAMVBYTE_SRCS}")
target_link_libraries(streamvbyte ${BASE_FLAGS})

set_target_properties(
    streamvbyte
    PROPERTIES
        VERSION "${STREAMVBYTE_LIB_VERSION}"
        SOVERSION "${STREAMVBYTE_LIB_SOVERSION}"
        WINDOWS_EXPORT_ALL_SYMBOLS YES
)
set_target_properties(
    streamvbyte_static
    PROPERTIES
        VERSION "${STREAMVBYTE_LIB_VERSION}"
        SOVERSION "${STREAMVBYTE_LIB_SOVERSION}"
        WINDOWS_EXPORT_ALL_SYMBOLS YES
)

target_include_directories(
    streamvbyte
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/include>
)
target_include_directories(
    streamvbyte_static
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/include>
)
install(
    FILES
        ${PROJECT_SOURCE_DIR}/include/streamvbyte.h
        ${PROJECT_SOURCE_DIR}/include/streamvbytedelta.h
        ${PROJECT_SOURCE_DIR}/include/streamvbyte_zigzag.h
    DESTINATION include
)
install(TARGETS streamvbyte streamvbyte_static DESTINATION lib)

option(STREAMVBYTE_SANITIZE_UNDEFINED "Sanitize undefined behavior" OFF)
if(STREAMVBYTE_SANITIZE_UNDEFINED)
    add_compile_options(-fsanitize=undefined -fno-sanitize-recover=all)
    add_link_options(-fsanitize=undefined -fno-sanitize-recover=all)
endif()

message(STATUS "CMAKE_SYSTEM_PROCESSOR: " ${CMAKE_SYSTEM_PROCESSOR})
message(STATUS "CMAKE_BUILD_TYPE: " ${CMAKE_BUILD_TYPE}) # this tends to be "sticky" so you can remain unknowingly in debug mode
message(STATUS "CMAKE_C_COMPILER: " ${CMAKE_C_COMPILER}) # important to know which compiler is used
message(STATUS "CMAKE_C_FLAGS: " ${CMAKE_C_FLAGS}) # important to know the flags
message(STATUS "CMAKE_C_FLAGS_DEBUG: " ${CMAKE_C_FLAGS_DEBUG})
message(STATUS "CMAKE_C_FLAGS_RELEASE: " ${CMAKE_C_FLAGS_RELEASE})

# build programs

# example
option(STREAMVBYTE_ENABLE_EXAMPLES "Enable examples for streamvbyte" OFF)
if(STREAMVBYTE_ENABLE_EXAMPLES)
    add_executable(example ${PROJECT_SOURCE_DIR}/example.c)
    target_link_libraries(example streamvbyte_static)
endif()

option(STREAMVBYTE_ENABLE_TESTS "Enable unit tests for streamvbyte" OFF)
if(STREAMVBYTE_ENABLE_TESTS)
    if(NOT MSVC)
        # perf
        add_executable(perf ${PROJECT_SOURCE_DIR}/tests/perf.c)
        target_link_libraries(perf streamvbyte_static)
        target_link_libraries(perf m)
    endif()

    # writeseq
    add_executable(writeseq ${PROJECT_SOURCE_DIR}/tests/writeseq.c)
    target_link_libraries(writeseq streamvbyte_static)

    # unit
    add_executable(unit ${PROJECT_SOURCE_DIR}/tests/unit.c)
    target_link_libraries(unit streamvbyte_static)

    enable_testing()

    # add unit tests
    add_test(NAME unit COMMAND unit)
    add_custom_target(check COMMAND ctest --output-on-failure DEPENDS unit)
endif()
