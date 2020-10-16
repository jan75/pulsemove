cmake_minimum_required(VERSION 3.15)
project(pulsemove VERSION "1.0.1")

set(CMAKE_CXX_STANDARD 11)

add_executable(pulsemove src/main.cpp src/pulsemovecontext.h src/pulsemovecontext.cpp)
target_link_libraries("pulsemove" pulse pthread)
