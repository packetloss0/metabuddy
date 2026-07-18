cmake_minimum_required(VERSION 3.16)
project(MetaBuddy LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

find_package(PkgConfig REQUIRED)
pkg_check_modules(TAGLIB REQUIRED IMPORTED_TARGET taglib)

add_executable(MetaBuddy
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.h
    src/tagio.cpp
    src/tagio.h
)

target_link_libraries(MetaBuddy PRIVATE Qt6::Widgets PkgConfig::TAGLIB)
