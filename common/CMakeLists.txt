cmake_minimum_required(VERSION 3.16)

add_library(workspace-common STATIC
  kwin_desktop.cpp
)

target_include_directories(workspace-common PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
)

find_package(Qt5 REQUIRED COMPONENTS Core DBus)

target_link_libraries(workspace-common PUBLIC
  Qt5::Core
  Qt5::DBus
)
