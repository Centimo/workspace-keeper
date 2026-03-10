cmake_minimum_required(VERSION 3.16)

add_library(workspace-common STATIC
  kwin_desktop.cpp
  claude_types.cpp
)

target_include_directories(workspace-common PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

find_package(Qt5 REQUIRED COMPONENTS Core DBus)

target_link_libraries(workspace-common PUBLIC
  Qt5::Core
  Qt5::DBus
)
