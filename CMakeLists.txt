cmake_minimum_required(VERSION 2.6)
project(twokinds)

set(CMAKE_MODULE_PATH "${twokinds_SOURCE_DIR}/cmake")

include(CheckCXXCompilerFlag)

check_cxx_compiler_flag("-std=c++11" HAVE_STD_CXX11)
if(HAVE_STD_CXX11)
    add_definitions("-std=c++11")
endif()

check_cxx_compiler_flag("-Wall" HAVE_WALL_SWITCH)
if(HAVE_WALL_SWITCH)
    add_definitions("-Wall")
endif()


find_package(SDL2 REQUIRED)
find_package(PhysFS REQUIRED)


set(HDRS src/engine.hpp
)

set(SRCS src/engine.cpp
         main.cpp
)

add_executable(twokinds ${SRCS} ${HDRS})
set_property(TARGET twokinds APPEND PROPERTY INCLUDE_DIRECTORIES
             "${twokinds_SOURCE_DIR}/src"
             ${SDL2_INCLUDE_DIR}
             ${PHYSFS_INCLUDE_DIR})
target_link_libraries(twokinds  ${SDL2_LIBRARY} ${PHYSFS_LIBRARY})

install(TARGETS twokinds RUNTIME DESTINATION bin)
