cmake_minimum_required(VERSION 3.10)
project(Ex-2)

set(CMAKE_CXX_STANDARD 17)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(RUN_UNITTEST "Enable unit-tests" OFF)
option(RUN_PROFILE "Enable profiling" OFF)

if(RUN_PROFILE)
    message("profiling enabled")
    add_definitions(-DRUN_PROFILER)
    set(CMAKE_BUILD_TYPE Debug)
endif()
if(RUN_UNITTEST)
    message("unit test enabled")
    add_definitions(-DRUN_CATCH)
endif()

set(CMAKE_CXX_FLAGS_RELEASE "-O3")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -Wall -Wextra")
# somehow santizer fails for valgrind
if(NOT RUN_PROFILE)
set(CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fsanitize=address")
endif()

SET( _SOURCES_

    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
    #${CMAKE_CURRENT_SOURCE_DIR}/src/gzio.cpp
)

SET( _HEADER_

    ${CMAKE_CURRENT_SOURCE_DIR}/hdr/gzio.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hdr/command.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hdr/Encoders.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hdr/Utility.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hdr/histogram.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hdr/catch.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/hdr/catch_testcases.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hdr/config.h
)

######################
#Include Definitions #
######################
#add_definitions(-DMEMORY_OPTIMIZED)

######################
#Include Directories #
######################
include_directories(${Boost_INCLUDE_DIRS})

######################
#link Directories    #
######################
link_directories(
    ${BOOST_LIBDIR}
    ${ZLIB_LIBRARIES}
)

find_package(ZLIB REQUIRED)
find_package(Boost REQUIRED COMPONENTS system iostreams program_options)

add_executable(${PROJECT_NAME} ${_SOURCES_} ${_HEADER_})

if (RUN_PROFILE)
    set(PROFILE_FLAGS
        -Wl,--no-as-needed
        -lprofiler
        -fno-pie
        -fno-omit-frame-pointer
        -fno-builtin-malloc -fno-builtin-calloc -fno-builtin-realloc -fno-builtin-free)
endif()

target_link_libraries(${PROJECT_NAME}
    -pthread
    -ltbb
    ${Boost_LIBRARIES}
    ZLIB::ZLIB
    ${PROFILE_FLAGS}
)
