cmake_minimum_required(VERSION 3.10)
project(N1 VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
add_compile_options(-O3)

set(PROJECT_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")

find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED 
    libavcodec 
    libavformat 
    libavutil 
    libswscale
)

if(FFMPEG_FOUND)
    message(STATUS "Found FFmpeg libraries:")
    message(STATUS "  Includes: ${FFMPEG_INCLUDE_DIRS}")
    message(STATUS "  Libraries: ${FFMPEG_LIBRARIES}")
    message(STATUS "  Cflags: ${FFMPEG_CFLAGS_OTHER}")
else()
    message(FATAL_ERROR "FFmpeg libraries not found. Install with: sudo apt-get install libavcodec-dev libavformat-dev libavutil-dev libswscale-dev")
endif()

include_directories(${PROJECT_ROOT_DIR}/Eigen)
include_directories(${FFMPEG_INCLUDE_DIRS})

add_definitions(
    -D__STDC_CONSTANT_MACROS
    ${FFMPEG_CFLAGS_OTHER}
)

file(GLOB_RECURSE HEADER_DIRS "${PROJECT_ROOT_DIR}/*.h")
set(DIR_LIST "")
foreach(header ${HEADER_DIRS})
    get_filename_component(DIR_PATH ${header} PATH)
    list(APPEND DIR_LIST ${DIR_PATH})
endforeach()
list(REMOVE_DUPLICATES DIR_LIST)
include_directories(${DIR_LIST})

file(GLOB_RECURSE SOURCES "${PROJECT_ROOT_DIR}/*.c*")
message(STATUS "Compiling the following sources: ${SOURCES}")

add_executable(V3 ${SOURCES})

target_link_libraries(V3
    ${FFMPEG_LIBRARIES}
)

if(FFMPEG_LIBRARY_DIRS)
    set_target_properties(V3 PROPERTIES
        LINK_FLAGS "-Wl,-rpath,${FFMPEG_LIBRARY_DIRS}"
    )
endif()