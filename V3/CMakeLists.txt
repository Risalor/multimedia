cmake_minimum_required(VERSION 3.10)
project(N1 VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
add_compile_options(-O3)

set(PROJECT_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")

# Find FFmpeg libraries using pkg-config (most reliable method on Linux)
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED 
    libavcodec 
    libavformat 
    libavutil 
    libswscale
)

# Check if FFmpeg was found
if(FFMPEG_FOUND)
    message(STATUS "Found FFmpeg libraries:")
    message(STATUS "  Includes: ${FFMPEG_INCLUDE_DIRS}")
    message(STATUS "  Libraries: ${FFMPEG_LIBRARIES}")
    message(STATUS "  Cflags: ${FFMPEG_CFLAGS_OTHER}")
else()
    message(FATAL_ERROR "FFmpeg libraries not found. Install with: sudo apt-get install libavcodec-dev libavformat-dev libavutil-dev libswscale-dev")
endif()

# Include Eigen from src directory (as per your existing setup)
include_directories(${PROJECT_ROOT_DIR}/Eigen)

# Include FFmpeg headers
include_directories(${FFMPEG_INCLUDE_DIRS})

# Add compile definitions for FFmpeg compatibility
add_definitions(
    -D__STDC_CONSTANT_MACROS
    ${FFMPEG_CFLAGS_OTHER}
)

# Include other headers as per your existing logic
file(GLOB_RECURSE HEADER_DIRS "${PROJECT_ROOT_DIR}/*.h")
set(DIR_LIST "")
foreach(header ${HEADER_DIRS})
    get_filename_component(DIR_PATH ${header} PATH)
    list(APPEND DIR_LIST ${DIR_PATH})
endforeach()
list(REMOVE_DUPLICATES DIR_LIST)
include_directories(${DIR_LIST})

# Find source files
file(GLOB_RECURSE SOURCES "${PROJECT_ROOT_DIR}/*.c*")
message(STATUS "Compiling the following sources: ${SOURCES}")

# Create executable
add_executable(N1 ${SOURCES})

# Link FFmpeg libraries
target_link_libraries(N1
    ${FFMPEG_LIBRARIES}
)

# Optional: Set runtime library path for FFmpeg if needed
if(FFMPEG_LIBRARY_DIRS)
    set_target_properties(N1 PROPERTIES
        LINK_FLAGS "-Wl,-rpath,${FFMPEG_LIBRARY_DIRS}"
    )
endif()

# Optional: Print build summary
message(STATUS "")
message(STATUS "=== Build Configuration Summary ===")
message(STATUS "Project: ${PROJECT_NAME}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Optimization: -O3")
message(STATUS "Eigen path: ${PROJECT_ROOT_DIR}/Eigen")
message(STATUS "FFmpeg libraries linked: ${FFMPEG_LIBRARIES}")
message(STATUS "====================================")