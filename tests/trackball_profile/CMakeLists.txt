cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(trackball_profile)

target_sources(app PRIVATE
  src/main.c
  ${CMAKE_CURRENT_LIST_DIR}/../../src/trackball_profile.c
)
target_include_directories(app PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../../include)
