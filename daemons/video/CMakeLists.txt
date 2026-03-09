cmake_minimum_required(VERSION 3.16)
project(video_daemon CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

add_executable(video_daemon src/video_daemon.cpp)
target_include_directories(video_daemon PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)
target_link_libraries(video_daemon PRIVATE Threads::Threads)
