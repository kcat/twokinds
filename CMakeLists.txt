cmake_minimum_required(VERSION 2.6)
project(twokinds)

add_executable(twokinds main.cpp)

install(TARGETS twokinds RUNTIME DESTINATION bin)
