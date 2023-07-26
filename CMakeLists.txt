cmake_minimum_required(VERSION 3.22.1)  # cmake lowest version requirement

project (cos_userspace) # project name

# include(FetchContent)
# FetchContent_Declare(
#   glog
#   URL https://github.com/google/glog/archive/refs/tags/v0.6.0.tar.gz
# )
# FetchContent_MakeAvailable(glog)

find_package (glog 0.6.0 REQUIRED)

# includes
include_directories(lib kernel) # global include directories

add_executable(fifo_scheduler scheduler/fifo/fifo_scheduler.cpp scheduler/fifo/fifo_lord.h)
target_link_libraries(fifo_scheduler glog::glog)

add_executable(shinjuku_scheduler scheduler/shinjuku/shinjuku_scheduler.cpp scheduler/shinjuku/shinjuku_lord.h)
target_link_libraries(shinjuku_scheduler glog::glog)

add_executable(simple_load workload/simple_load.cpp)

# cmake flags
# set(CMAKE_BUILD_TYPE "Debug")
set(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g -ggdb")
set(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall")
