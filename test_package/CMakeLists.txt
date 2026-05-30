cmake_minimum_required(VERSION 3.14)
project(test_package LANGUAGES CXX)

find_package(H5CPPCompiler REQUIRED)

add_executable(example src/example.cpp)
