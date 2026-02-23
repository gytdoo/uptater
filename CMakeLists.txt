cmake_minimum_required(VERSION 3.16)
project(uptater LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(QTERM REQUIRED qtermwidget6)

add_executable(uptater
    main.cpp
    commandrunner.cpp
    commandrunner.h
    packagemanager.cpp
    packagemanager.h
    mainwindow.h
    mainwindow.cpp
    terminalwindow.cpp
    terminalwindow.h
    buttonpanel.cpp
    buttonpanel.h
    depcheck.cpp
    depcheck.h
    aboutdialog.h
    aboutdialog.cpp
    pacmanconfigmanager.h
    pacmanconfigmanager.cpp
    reflectormanager.h
    reflectormanager.cpp
    dashboardwidget.h
    dashboardwidget.cpp
    resources.qrc
)

target_include_directories(uptater PRIVATE ${QTERM_INCLUDE_DIRS})
target_link_libraries(uptater PRIVATE Qt6::Widgets ${QTERM_LIBRARIES})
