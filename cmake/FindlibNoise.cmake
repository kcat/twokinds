# - Find libnoise includes and libraries
#
#   LIBNOISE_FOUND        - True if LIBNOISE_INCLUDE_DIR & LIBNOISE_LIBRARY are found
#   LIBNOISE_LIBRARIES    - where to find libnoise/noise.h, etc.
#   LIBNOISE_INCLUDE_DIRS - the libnoise library
#

find_path(LIBNOISE_INCLUDE_DIR
          NAMES libnoise/noise.h
          DOC "The libnoise include directory"
)

find_library(LIBNOISE_LIBRARY
             NAMES noise
             DOC "The libnoise library"
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(libNoise
    REQUIRED_VARS LIBNOISE_LIBRARY LIBNOISE_INCLUDE_DIR
)

if(LIBNOISE_FOUND)
    set(LIBNOISE_LIBRARIES ${LIBNOISE_LIBRARY})
    set(LIBNOISE_INCLUDE_DIRS ${LIBNOISE_INCLUDE_DIR})
endif()

mark_as_advanced(LIBNOISE_INCLUDE_DIR LIBNOISE_LIBRARY)
