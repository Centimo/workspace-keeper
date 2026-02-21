cmake_minimum_required(VERSION 3.20)
project(workspace-menu LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network)
find_package(PkgConfig REQUIRED)
pkg_check_modules(XCB REQUIRED xcb)

add_executable(workspace-menu
  src/main.cpp
  src/workspace_model.cpp
  src/workspace_menu.cpp
  src/menu_window.cpp
  src/daemon_server.cpp
)

target_include_directories(workspace-menu PRIVATE ${XCB_INCLUDE_DIRS})

target_link_libraries(workspace-menu PRIVATE
  Qt6::Core
  Qt6::Gui
  Qt6::Widgets
  Qt6::Network
  ${XCB_LIBRARIES}
)
