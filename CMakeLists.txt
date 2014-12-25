cmake_minimum_required(VERSION 2.6)
project(twokinds)

include(CheckCXXCompilerFlag)

check_cxx_compiler_flag("-std=c++11" HAVE_STD_CXX11)
if(HAVE_STD_CXX11)
    add_definitions("-std=c++11")
endif()

check_cxx_compiler_flag("-Wall" HAVE_WALL_SWITCH)
if(HAVE_WALL_SWITCH)
    add_definitions("-Wall")
endif()


add_executable(twokinds main.cpp)

install(TARGETS twokinds RUNTIME DESTINATION bin)
