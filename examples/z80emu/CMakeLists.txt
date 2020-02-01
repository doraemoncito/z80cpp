cmake_minimum_required(VERSION 3.5.1)
project(z80emu LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(INSTALL_EXAMPLEDIR "examples")

include_directories(
    ../../include
)

add_executable( z80emu
    shared/zexall.h
    shared/SpectrumScreen.cpp
    shared/SpectrumScreen.cpp
    shared/z80emu.h
    shared/z80emu.cpp
    ../../src/z80.cpp
)

# Use Qt and our Circle compatibility library in MacOS builds
if( ${CMAKE_SYSTEM_NAME} MATCHES "Darwin" )

    find_package( Qt5 COMPONENTS Widgets REQUIRED )

    target_sources( z80emu PRIVATE
        macos/main.cpp
        macos/screen.cpp
        macos/screen.h
        macos/window.cpp
        macos/window.h
        ../../src/z80.cpp
    )

    include_directories(BEFORE ../../compatibility)
    target_link_libraries( z80emu PUBLIC Qt5::Widgets )

endif()

if( ${CMAKE_SYSTEM_NAME} MATCHES "Generic" )

    # Stop CMake from linking against 'arm-none-eabi/lib/libstdc++.a'
    set(CMAKE_CXX_IMPLICIT_LINK_LIBRARIES "")
    set(CMAKE_CXX_IMPLICIT_LINK_DIRECTORIES "")

    # FIXME: make this relative
    set( INCLUDE_PREFIX "/home/vagrant/src/z80cpp/build" )
#    set( NEWLIB_ARCH "arm-none-circle" )
    set( NEWLIB_ARCH "aarch64-none-circle" )
    set( NEWLIBDIR "${INCLUDE_PREFIX}/../../circle-stdlib/install/${NEWLIB_ARCH}" )
    set( CIRCLEHOME "${INCLUDE_PREFIX}/../../circle-stdlib/libs/circle" )

    include_directories( BEFORE
        ${NEWLIBDIR}/include
        ${CIRCLEHOME}/include
    )

    include_directories(
        shared
    )

    target_include_directories( z80emu PRIVATE
        raspberrypi
    )

    set( LIBS
        "${NEWLIBDIR}/lib/libm.a"
        "${NEWLIBDIR}/lib/libc.a"
        "${NEWLIBDIR}/lib/libcirclenewlib.a"
        "${CIRCLEHOME}/addon/SDCard/libsdcard.a"
        "${CIRCLEHOME}/lib/usb/libusb.a"
        "${CIRCLEHOME}/lib/input/libinput.a"
        "${CIRCLEHOME}/lib/fs/fat/libfatfs.a"
        "${CIRCLEHOME}/lib/fs/libfs.a"
        "${CIRCLEHOME}/lib/net/libnet.a"
        "${CIRCLEHOME}/lib/sched/libsched.a"
        "${CIRCLEHOME}/lib/libcircle.a"
        "/usr/local/opt/gcc-arm-8.3-2019.03-x86_64-aarch64-elf/aarch64-elf/lib/libstdc++.a"
        "/usr/local/opt/gcc-arm-8.3-2019.03-x86_64-aarch64-elf/lib/gcc/aarch64-elf/8.3.0/libgcc.a"
#        "/usr/local/opt/gcc-arm-9.2-2019.12-x86_64-arm-none-eabi/bin/../lib/gcc/arm-none-eabi/9.2.1/../../../../arm-none-eabi/lib/arm/v5te/hard/libstdc++.a"
#        "/usr/local/opt/gcc-arm-9.2-2019.12-x86_64-arm-none-eabi/bin/../lib/gcc/arm-none-eabi/9.2.1/arm/v5te/hard/libgcc.a"
    )

    set( EXTRALIBS
        z80cpp-static
    )

    target_sources( z80emu PRIVATE
        raspberrypi/main.cpp
        raspberrypi/kernel.cpp
    )

    set( LOADADDR 0x80000 )

    # https://gcc.gnu.org/onlinedocs/gcc/Link-Options.html
    # After printing the whole g++ command like by invoking it with the "-v" argument we can see that g++
    # adds some additional C runtime libraries to the ld command line, namely: crtbegin, crti and crt0.
    # One of thoses files references the 'bss_start' and 'bss_end' symbols and since we don't need any
    # of thoses files for linking we can remove them from the linker command line by passing the "-nostdlib"
    # argument to g++.  For more details please see this link:
    # https://stackoverflow.com/questions/57357726/linking-failed-when-use-indirectly-the-gnu-linker
    #
    target_link_libraries( z80emu PUBLIC
        "-nostdlib"
        "-T ${CIRCLEHOME}/circle.ld"
        "-Wl,--section-start=.init=${LOADADDR}"
        "-Wl,--start-group"
        ${LIBS}
        ${EXTRALIBS}
        "-Wl,--end-group"
    )

    # post cmake 3.13 we ca use
    #target_link_options( z80screen PUBLIC "-T $(CIRCLEHOME)/circle.ld" )

endif()

install(TARGETS z80emu
    RUNTIME DESTINATION "${INSTALL_EXAMPLEDIR}"
    BUNDLE DESTINATION "${INSTALL_EXAMPLEDIR}"
    LIBRARY DESTINATION "${INSTALL_EXAMPLEDIR}"
)
