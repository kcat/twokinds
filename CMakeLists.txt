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


if(WIN32)
    set(CMAKE_MODULE_PATH "$ENV{OGRE_HOME}/CMake/" "${CMAKE_MODULE_PATH}")
    set(OGRE_SAMPLES_INCLUDEPATH "$ENV{OGRE_HOME}/Samples/include")
endif(WIN32)
if(UNIX)
    if(EXISTS "/usr/local/lib/OGRE/cmake")
        # We could just *assume* that developers uses this basepath : /usr/local
        set(CMAKE_MODULE_PATH "/usr/local/lib/OGRE/cmake/" "${CMAKE_MODULE_PATH}")
        set(OGRE_SAMPLES_INCLUDEPATH "/usr/local/share/OGRE/samples/Common/include/")
    elseif(EXISTS "/usr/lib/OGRE/cmake")
        # Otherwise, this one
        set(CMAKE_MODULE_PATH "/usr/lib/OGRE/cmake/" "${CMAKE_MODULE_PATH}")
        set(OGRE_SAMPLES_INCLUDEPATH "/usr/share/OGRE/samples/Common/include/")
    else()
        message(SEND_ERROR "Failed to find module path.")
    endif()
endif(UNIX)

if(NOT CMAKE_BUILD_TYPE)
    # CMake defaults to leaving CMAKE_BUILD_TYPE empty. This screws up
    # differentiation between debug and release builds.
    set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "Choose the type of build, options are: None (CMAKE_CXX_FLAGS or CMAKE_C_FLAGS used) Debug Release RelWithDebInfo MinSizeRel." FORCE)
endif()

set(CMAKE_DEBUG_POSTFIX "_d")

find_package(OGRE REQUIRED)
find_package(SDL2 REQUIRED)
find_package(MyGUI REQUIRED)
find_package(PhysFS REQUIRED)
find_package(libNoise REQUIRED)

if(NOT OGRE_RTShaderSystem_FOUND)
    message(FATAL_ERROR "Failed to find Ogre RTShaderSystem component")
endif()

# Find Boost
if(NOT OGRE_BUILD_PLATFORM_IPHONE)
    if(WIN32 OR APPLE)
        set(Boost_USE_STATIC_LIBS TRUE)
    else()
        # Statically linking boost to a dynamic Ogre build doesn't work on Linux 64bit
        set(Boost_USE_STATIC_LIBS ${OGRE_STATIC})
    endif()
    if(MINGW)
        # this is probably a bug in CMake: the boost find module tries to look for
        # boost libraries with name libboost_*, but CMake already prefixes library
        # search names with "lib". This is the workaround.
        set(CMAKE_FIND_LIBRARY_PREFIXES ${CMAKE_FIND_LIBRARY_PREFIXES} "")
    endif()
    set(Boost_ADDITIONAL_VERSIONS "1.44" "1.44.0" "1.42" "1.42.0" "1.41.0" "1.41" "1.40.0" "1.40" "1.39.0" "1.39" "1.38.0" "1.38" "1.37.0" "1.37" )
    # Components that need linking (NB does not include header-only components like bind)
    set(OGRE_BOOST_COMPONENTS thread date_time)
    find_package(Boost COMPONENTS ${OGRE_BOOST_COMPONENTS} QUIET)
    if(NOT Boost_FOUND)
        # Try again with the other type of libs
        set(Boost_USE_STATIC_LIBS NOT ${Boost_USE_STATIC_LIBS})
        find_package(Boost COMPONENTS ${OGRE_BOOST_COMPONENTS} QUIET)
    endif()
    find_package(Boost QUIET)

    # Set up referencing of Boost
    add_definitions(-DBOOST_ALL_NO_LIB)
    set(OGRE_INCLUDE_DIRS ${OGRE_INCLUDE_DIRS} ${Boost_INCLUDE_DIR})
    set(OGRE_LIBRARIES ${OGRE_LIBRARIES} ${Boost_LIBRARIES})
endif()


configure_file("${twokinds_SOURCE_DIR}/plugins.cfg.in"
               "${CMAKE_BINARY_DIR}/plugins.cfg")

set(DATA_ROOT  "${twokinds_SOURCE_DIR}/data")
configure_file("${twokinds_SOURCE_DIR}/resources.cfg.in"
               "${CMAKE_BINARY_DIR}/resources.cfg")


set(HDRS src/archives/physfs.hpp
         src/noiseutils/noiseutils.h
         src/input/iface.hpp
         src/input/input.hpp
         src/gui/iface.hpp
         src/gui/gui.hpp
         src/terrain/buffercache.hpp
         src/terrain/chunk.hpp
         src/terrain/defaultworld.hpp
         src/terrain/defs.hpp
         src/terrain/material.hpp
         src/terrain/quadtreenode.hpp
         src/terrain/storage.hpp
         src/terrain/terraingrid.hpp
         src/terrain/world.hpp
         src/terrain.hpp
         src/log.hpp
         src/cvars.hpp
         src/engine.hpp
         src/delegates.hpp
         src/singleton.hpp
         src/sparsearray.hpp
)

set(SRCS src/archives/physfs.cpp
         src/noiseutils/noiseutils.cpp
         src/input/input.cpp
         src/gui/gui.cpp
         src/terrain/buffercache.cpp
         src/terrain/chunk.cpp
         src/terrain/defaultworld.cpp
         src/terrain/material.cpp
         src/terrain/quadtreenode.cpp
         src/terrain/storage.cpp
         src/terrain/terraingrid.cpp
         src/terrain/world.cpp
         src/terrain.cpp
         src/log.cpp
         src/cvars.cpp
         src/engine.cpp
         main.cpp
)

add_executable(twokinds ${SRCS} ${HDRS})
set_property(TARGET twokinds APPEND PROPERTY INCLUDE_DIRECTORIES
    "${twokinds_SOURCE_DIR}/src"
    ${OGRE_INCLUDE_DIRS}
    ${OGRE_RTShaderSystem_INCLUDE_DIRS}
    ${SDL2_INCLUDE_DIR}
    ${MYGUI_INCLUDE_DIRS}
    ${PHYSFS_INCLUDE_DIR}
    ${LIBNOISE_INCLUDE_DIRS}
)
target_link_libraries(twokinds
    ${OGRE_LIBRARIES}
    ${OGRE_RTShaderSystem_LIBRARIES}
    ${SDL2_LIBRARY}
    ${MYGUI_PLATFORM_LIBRARIES}
    ${MYGUI_LIBRARIES}
    ${PHYSFS_LIBRARY}
    ${LIBNOISE_LIBRARIES}
)

install(TARGETS twokinds RUNTIME DESTINATION bin)
