cmake_minimum_required(VERSION 3.16)
project(z80emu LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(INSTALL_EXAMPLEDIR "examples")

find_package(Qt5 COMPONENTS Widgets REQUIRED)

include_directories(
    ../../include
)

add_executable(z80emu
    macos/main.cpp
    macos/screen.cpp macos/screen.h
    macos/window.cpp macos/window.h
    shared/zexall.h shared/SpectrumScreen.cpp shared/SpectrumScreen.cpp
    shared/z80emu.h shared/z80emu.cpp
    circle/bcmframebuffer.h circle/util.h circle/types.h
    ../../src/z80.cpp
)

target_link_libraries(z80emu PUBLIC Qt5::Widgets)

install(TARGETS z80emu
    RUNTIME DESTINATION "${INSTALL_EXAMPLEDIR}"
    BUNDLE DESTINATION "${INSTALL_EXAMPLEDIR}"
    LIBRARY DESTINATION "${INSTALL_EXAMPLEDIR}"
)
