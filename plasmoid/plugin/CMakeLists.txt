cmake_minimum_required(VERSION 3.16)
project(workspace-monitor-plugin LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt5 REQUIRED COMPONENTS Core DBus Qml)

add_library(workspacemonitorplugin SHARED
  src/workspace_monitor.cpp
  src/plugin.cpp
)

target_link_libraries(workspacemonitorplugin PRIVATE
  Qt5::Core
  Qt5::DBus
  Qt5::Qml
)
