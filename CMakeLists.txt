cmake_minimum_required(VERSION 3.3)
set(CMAKE_MACOSX_RPATH OFF)
if (NOT CMAKE_BUILD_TYPE)
                message(STATUS "No build type selected, default to Release")
                set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
endif()

project(STREAMVBYTE VERSION "0.0.1")
cmake_policy(SET CMP0065 OLD)
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)
set(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} -v" )
set(BASE_FLAGS
  "-std=c99"
  "-fPIC"
  "-Wextra"
  "-pedantic"
  "-Wshadow"
)
if(CMAKE_BUILD_TYPE MATCHES Debug)
  set(BASE_FLAGS
    ${BASE_FLAGS}
    "-O0"
    "-ggdb"
    )
else()
  set(BASE_FLAGS
    ${BASE_FLAGS}
    "-O3"
    "-g"
    )
endif()
if (MSVC)
    add_definitions(
        "-D__restrict__=__restrict"
        )
endif()
# test for arm
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64.*|AARCH64.*)")
   set(BASE_FLAGS
    ${BASE_FLAGS}
    "-D__ARM_NEON__"
    )
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
target_include_directories(
  streamvbyte
  PUBLIC ${PROJECT_SOURCE_DIR}/include
)
target_include_directories(
  streamvbyte_static
  PUBLIC ${PROJECT_SOURCE_DIR}/include
)
install(FILES
  ${PROJECT_SOURCE_DIR}/include/streamvbyte.h
  ${PROJECT_SOURCE_DIR}/include/streamvbytedelta.h
  ${PROJECT_SOURCE_DIR}/include/streamvbyte_zigzag.h
   DESTINATION include
  )
install(
  TARGETS streamvbyte streamvbyte_static
  DESTINATION lib)
## -march=native is not supported on some platforms
if(NOT MSVC)
if(NOT STREAMVBYTE_DISABLE_NATIVE)
set(OPT_FLAGS "-march=native")
endif()
endif()

set(CMAKE_C_FLAGS "${STD_FLAGS} ${OPT_FLAGS} ${INCLUDE_FLAGS} ${WARNING_FLAGS} ${SANITIZE_FLAGS} ")

MESSAGE( STATUS "CMAKE_SYSTEM_PROCESSOR: " ${CMAKE_SYSTEM_PROCESSOR})
MESSAGE( STATUS "CMAKE_BUILD_TYPE: " ${CMAKE_BUILD_TYPE} ) # this tends to be "sticky" so you can remain unknowingly in debug mode
MESSAGE( STATUS "CMAKE_C_COMPILER: " ${CMAKE_C_COMPILER} ) # important to know which compiler is used
MESSAGE( STATUS "CMAKE_C_FLAGS: " ${CMAKE_C_FLAGS} ) # important to know the flags
MESSAGE( STATUS "CMAKE_C_FLAGS_DEBUG: " ${CMAKE_C_FLAGS_DEBUG} )
MESSAGE( STATUS "CMAKE_C_FLAGS_RELEASE: " ${CMAKE_C_FLAGS_RELEASE} )

# build programs

# example
add_executable (example ${PROJECT_SOURCE_DIR}/example.c)
target_link_libraries (example streamvbyte_static)

if(NOT MSVC)
    # perf
    add_executable (perf ${PROJECT_SOURCE_DIR}/tests/perf.c)
    target_link_libraries (perf streamvbyte_static)
    target_link_libraries(perf m)
endif()
# writeseq
add_executable (writeseq ${PROJECT_SOURCE_DIR}/tests/writeseq.c)
target_link_libraries (writeseq streamvbyte_static)
# unit
add_executable (unit ${PROJECT_SOURCE_DIR}/tests/unit.c)
target_link_libraries (unit streamvbyte_static)

option(STREAMVBYTE_ENABLE_TESTS "enable unit tests for streamvbyte" ON)
if(STREAMVBYTE_ENABLE_TESTS)
    enable_testing()
    # add unit tests
    add_test(NAME unit COMMAND unit)
    add_custom_target(check
        COMMAND ctest --output-on-failure
    DEPENDS unit)
endif()
