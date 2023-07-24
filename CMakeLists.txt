cmake_minimum_required(VERSION 3.22.1)  # cmake lowest version requirement

project (cos_userspace) # project name

set(CMAKE_CXX_STANDARD 14)  # GoogleTest requires at least C++14
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Third-parties
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
FetchContent_MakeAvailable(googletest)

find_package (glog 0.6.0 REQUIRED)

# includes
include_directories(lib kernel) # global include directories

# cmake flags
# set(CMAKE_BUILD_TYPE "Debug")
set(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g -ggdb")
set(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall")
