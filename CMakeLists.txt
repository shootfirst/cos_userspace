cmake_minimum_required(VERSION 3.22.1)  # cmake lowest version requirement

project (cos_userspace) # project name

# Third-parties
include(FetchContent)
FetchContent_Declare(
  glog
  URL https://github.com/google/glog/archive/refs/tags/v0.6.0.tar.gz
)
FetchContent_MakeAvailable(glog)

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.13.0.zip
)
FetchContent_MakeAvailable(googletest)

# options
option(ENABLE_MQ_TEST "Enable the test" ON)
if(ENABLE_MQ_TEST)
    add_definitions(-DMQ_TEST)
endif()

find_path(ROCKSDB_INCLUDE_DIR rocksdb/db.h PATHS)
include_directories(${ROCKSDB_INCLUDE_DIR})

find_library(ROCKSDB_LIB rocksdb)
message(STATUS "finding ${ROCKSDB_LIB}")

find_library(LZ4_LIB lz4)
message(STATUS "finding ${LZ4_LIB}")

# includes
include_directories(lib kernel) # global include directories

add_executable(fifo_scheduler scheduler/fifo/fifo_scheduler.cpp scheduler/fifo/fifo_lord.h)
target_link_libraries(fifo_scheduler glog::glog)

add_executable(shinjuku_scheduler scheduler/shinjuku/shinjuku_scheduler.cpp scheduler/shinjuku/shinjuku_lord.h)
target_link_libraries(shinjuku_scheduler glog::glog)

add_executable(rocksdb workload/rocksdb.cpp)
target_link_libraries(rocksdb glog::glog)
target_link_libraries(rocksdb ${ROCKSDB_LIB} ${LZ4_LIB} -ltbb -lstdc++fs -lrocksdb -lzstd -lz -lsnappy -lbz2)

add_executable(task_delegation_latency_load workload/task_delegation_latency_load.cpp)
add_executable(cgroup_latency_load workload/cgroup_latency_load.cpp)

# for tests
enable_testing()
add_executable(simple_exp workload/simple_exp.cpp)
target_link_libraries(simple_exp GTest::gtest_main)
include(GoogleTest)
gtest_discover_tests(simple_exp)

# cmake flags
# set(CMAKE_BUILD_TYPE "Debug")
set(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g -ggdb")
set(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall")
