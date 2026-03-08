cmake_minimum_required(VERSION 3.16)
project(aht30_daemon CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find threading library (required by httplib)
find_package(Threads REQUIRED)

include_directories(include)

# Sensor library
add_library(aht30 src/aht30.cpp)

# Optional: test tool (used in Step 4)
add_executable(test_read src/test_read.cpp)
target_link_libraries(test_read PRIVATE aht30)

# Daemon executable (introduced in Step 5)
# This guard prevents CMake from failing in Step 4 before the daemon source exists.
if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/aht30_daemon.cpp)
    add_executable(aht30_daemon src/aht30_daemon.cpp)

    target_include_directories(aht30_daemon PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party
    )

    target_link_libraries(aht30_daemon PRIVATE aht30 Threads::Threads)
endif()
