cmake_minimum_required(VERSION 3.10)
project(N1 VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
add_compile_options(-O3)

set(PROJECT_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")

include_directories(${PROJECT_ROOT_DIR}/Eigen)

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

add_executable(V2 ${SOURCES})