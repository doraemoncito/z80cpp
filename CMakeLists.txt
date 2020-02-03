# By default, CMake generates standard UNIX makefiles.  To generate an XCode
# project on MacOS, invoke CMake from the build directory as shown below:
#
#   cmake -G Xcode ..
#

cmake_minimum_required( VERSION 3.5.1 FATAL_ERROR )
project( z80cpp LANGUAGES C CXX )

# For the bare metal Raspberry Pi system we can only build the static library
if( ${CMAKE_SYSTEM_NAME} MATCHES "Generic" )
    set( Z80CPP_STATIC_ONLY true )
endif()

# https://stackoverflow.com/questions/5096881/does-set-target-properties-in-cmake-override-cmake-cxx-flags
# https://gitlab.kitware.com/cmake/cmake/issues/17991
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${CFLAGS_FOR_TARGET}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CFLAGS_FOR_TARGET}")
#set_target_properties(${TARGET} PROPERTIES COMPILE_FLAGS ${BUILD_FLAGS})

# Set the rpath policy and C++ version for MacOS builds
if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    # set the RPATH policy
    set(CMAKE_MACOSX_RPATH 1)

    # specify the C++ standard
    set(CMAKE_CXX_STANDARD 14)
    set(CMAKE_CXX_STANDARD_REQUIRED True)
endif()

set (API_REVISION 1)
set (VERSION_MAJOR 0)
set (VERSION_MINOR 0)
set (RELEASE_TYPE "")
set (VERSION_STR "${API_REVISION}.${VERSION_MAJOR}.${VERSION_MINOR}${RELEASE_TYPE}")

if (CMAKE_COMPILER_IS_GNUCXX)
    set (CMAKE_CXX_FLAGS "-Wall -O3 -std=c++14 ${CFLAGS_FOR_TARGET}")
endif ()

include_directories(BEFORE . include)

add_subdirectory(examples)

set (z80cpp_sources src/z80.cpp include/z80.h include/z80operations.h )
add_library (z80cpp-static STATIC ${z80cpp_sources})
set_target_properties (z80cpp-static PROPERTIES OUTPUT_NAME z80cpp)
if (NOT DEFINED Z80CPP_STATIC_ONLY)
    add_library (z80cpp SHARED ${z80cpp_sources})
    # Affects Win32 only: avoid dynamic/static *.lib files naming conflict 
    set_target_properties (z80cpp-static PROPERTIES PREFIX "lib")
endif ()

if (NOT DEFINED Z80CPP_STATIC_ONLY)
    set_target_properties(z80cpp
        PROPERTIES VERSION ${VERSION_STR} SOVERSION ${API_REVISION}
    )
endif ()

set_target_properties(z80cpp-static
    PROPERTIES VERSION ${VERSION_STR} SOVERSION ${API_REVISION}
)

if ("${CMAKE_CPP_IMPLICIT_LINK_DIRECTORIES}" MATCHES "lib64")
    set (LIB_DIR "lib64")
else ()
    set (LIB_DIR "lib")
endif ()
if (NOT DEFINED Z80CPP_STATIC_ONLY)
    install (TARGETS z80cpp LIBRARY DESTINATION ${LIB_DIR} ARCHIVE DESTINATION ${LIB_DIR})
endif ()

if( ${CMAKE_SYSTEM_NAME} MATCHES "Generic" )

    # Stop CMake from linking against 'arm-none-eabi/lib/libstdc++.a'
    set(CMAKE_CXX_IMPLICIT_LINK_LIBRARIES "")
    set(CMAKE_CXX_IMPLICIT_LINK_DIRECTORIES "")

    # FIXME: make this relative
    set( INCLUDE_PREFIX "/home/vagrant/src/z80cpp/build" )
    set( NEWLIB_ARCH "arm-none-circle" )
#    set( NEWLIB_ARCH "aarch64-none-circle" )
    set( NEWLIBDIR "${INCLUDE_PREFIX}/../../circle-stdlib/install/${NEWLIB_ARCH}" )
    set( CIRCLEHOME "${INCLUDE_PREFIX}/../../circle-stdlib/libs/circle" )

    include_directories( BEFORE
        ${NEWLIBDIR}/include
        ${CIRCLEHOME}/include
    )

    set( CIRCLE_LIBS
        ${NEWLIBDIR}/lib/libm.a
        ${NEWLIBDIR}/lib/libc.a
        ${NEWLIBDIR}/lib/libcirclenewlib.a
        ${CIRCLEHOME}/addon/SDCard/libsdcard.a
        ${CIRCLEHOME}/lib/usb/libusb.a
        ${CIRCLEHOME}/lib/input/libinput.a
        ${CIRCLEHOME}/lib/fs/fat/libfatfs.a
        ${CIRCLEHOME}/lib/fs/libfs.a
        ${CIRCLEHOME}/lib/net/libnet.a
        ${CIRCLEHOME}/lib/sched/libsched.a
        ${CIRCLEHOME}/lib/libcircle.a
    )
else()
    include_directories(BEFORE compatibility)
endif()

# List include directories for debugging
# https://stackoverflow.com/questions/6902149/listing-include-directories-in-cmake
get_property( dirs DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY INCLUDE_DIRECTORIES )
foreach( dir ${dirs} )
  message( STATUS "Include directory: '${dir}'" )
endforeach()

# Enable running the test simulator as part the `make test` target
set( TEST_SOURCES
    example/z80sim.cpp
    example/z80sim.h
)

#add_executable( z80sim ${TEST_SOURCES} )
#target_link_libraries( z80sim z80cpp-static ${CIRCLE_LIBS} )
#
#if( ${CMAKE_SYSTEM_NAME} MATCHES "Generic" )
#    # Stop CMake from linking against 'arm-none-eabi/lib/libstdc++.a'
#    set_target_properties( z80sim PROPERTIES LINKER_LANGUAGE CXX )
#endif()
#
#configure_file( example/zexall.bin zexall.bin COPYONLY )
#
#enable_testing( ) 
#add_test( NAME z80sim COMMAND z80sim )

install( TARGETS z80cpp-static LIBRARY DESTINATION ${LIB_DIR} ARCHIVE DESTINATION ${LIB_DIR} )
install( DIRECTORY include/ DESTINATION include/z80cpp PATTERN "*.h" )
