cmake_minimum_required(VERSION 3.20.0)

option(TRACKBALL_STUDIO_HOST_TEST "Build the standalone Studio RPC host test" OFF)

if(TRACKBALL_STUDIO_HOST_TEST)
  project(trackball_studio_host)
  enable_testing()

  add_executable(trackball_studio_host
    src/main.c
    ${CMAKE_CURRENT_LIST_DIR}/../../src/trackball_profile.c
    ${CMAKE_CURRENT_LIST_DIR}/../../src/trackball_settings.c
    ${CMAKE_CURRENT_LIST_DIR}/../../src/studio/trackball_settings_handler.c
  )
  target_compile_definitions(trackball_studio_host PRIVATE
    TRACKBALL_SETTINGS_HOST_TEST
    TRACKBALL_STUDIO_HOST_TEST
  )
  target_include_directories(trackball_studio_host PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../../include)
  add_test(NAME trackball_studio_host COMMAND trackball_studio_host)
  return()
endif()

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(studio_trackball_settings)

target_sources(app PRIVATE
  src/main.c
  ${CMAKE_CURRENT_LIST_DIR}/../../src/trackball_profile.c
  ${CMAKE_CURRENT_LIST_DIR}/../../src/trackball_settings.c
  ${CMAKE_CURRENT_LIST_DIR}/../../src/studio/trackball_settings_handler.c
)
target_compile_definitions(app PRIVATE
  TRACKBALL_SETTINGS_TEST_ADAPTER
  TRACKBALL_STUDIO_ZTEST
)
target_include_directories(app PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../../include)
