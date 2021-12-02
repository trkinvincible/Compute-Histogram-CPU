cmake_minimum_required(VERSION 3.10)
project(Ex-2)

set(CMAKE_CXX_STANDARD 17)
#set(CMAKE_CXX_FLAGS "-pg") - use only with GCC 's own profiler

#list (APPEND CMAKE_P.REFIX_PATH "/media/trk/DATA/Code-Factory/NUTO/externals_libs")

SET( _SOURCES_

    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gzio.cpp
)

SET( _HEADER_

    ${CMAKE_CURRENT_SOURCE_DIR}/hdr/Improved.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hdr/gzio.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hdr/command.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hdr/Encoders.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hdr/Utility.h
     ${CMAKE_CURRENT_SOURCE_DIR}/hdr/histogram.h
)

######################
#Include Definitions #
######################
#add_definitions(-DMEMORY_OPTIMIZED)

######################
#Include Directories #
######################
#include_directories(
#    )

######################
#link Directories    #
######################
link_directories(
    ${BOOST_LIBDIR}
    ${ZLIB_LIBRARIES}
    )

find_package(ZLIB REQUIRED)
find_package(Boost REQUIRED COMPONENTS system iostreams program_options)

include_directories(${Boost_INCLUDE_DIRS})

add_executable(${PROJECT_NAME} ${_SOURCES_} ${_HEADER_})

#set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
#set (CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")

target_link_libraries(${PROJECT_NAME}
    -latomic -lboost_system -pthread
    ${Boost_LIBRARIES}
    ZLIB::ZLIB
    -lboost_iostreams
    )
