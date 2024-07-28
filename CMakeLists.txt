cmake_minimum_required(VERSION 3.0)

set (app_name pcie_enum)
project (${app_name})

set(CMAKE_CXX_FLAGS "-std=c++20")
SET(CMAKE_BUILD_TYPE "Debug")

find_package(FMT REQUIRED)

add_executable(${app_name} main.cpp)