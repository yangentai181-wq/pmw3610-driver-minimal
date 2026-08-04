cmake_minimum_required(VERSION 3.20.0)

option(TRACKBALL_SETTINGS_HOST_TEST "Build the standalone host transaction test" OFF)

if(TRACKBALL_SETTINGS_HOST_TEST)
  project(trackball_settings_host)
  enable_testing()

  add_executable(trackball_settings_host
    src/main.c
    ${CMAKE_CURRENT_LIST_DIR}/../../src/trackball_profile.c
    ${CMAKE_CURRENT_LIST_DIR}/../../src/trackball_settings.c
  )
  target_compile_definitions(trackball_settings_host PRIVATE TRACKBALL_SETTINGS_HOST_TEST)
  target_include_directories(trackball_settings_host PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../../include)
  add_test(NAME trackball_settings_host COMMAND trackball_settings_host)
  return()
endif()

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(trackball_settings)

target_sources(app PRIVATE
  src/main.c
  ${CMAKE_CURRENT_LIST_DIR}/../../src/trackball_profile.c
  ${CMAKE_CURRENT_LIST_DIR}/../../src/trackball_settings.c
)
target_compile_definitions(app PRIVATE TRACKBALL_SETTINGS_TEST_ADAPTER)
target_include_directories(app PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../../include)
