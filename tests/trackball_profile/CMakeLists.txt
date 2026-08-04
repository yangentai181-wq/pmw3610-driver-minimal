cmake_minimum_required(VERSION 3.20.0)

option(TRACKBALL_PROFILE_HOST_TEST "Build the standalone host profile test" OFF)

if(TRACKBALL_PROFILE_HOST_TEST)
  project(trackball_profile_host)
  enable_testing()

  add_executable(trackball_profile_host
    src/main.c
    ${CMAKE_CURRENT_LIST_DIR}/../../src/trackball_profile.c
  )
  target_compile_definitions(trackball_profile_host PRIVATE TRACKBALL_PROFILE_HOST_TEST)
  target_include_directories(trackball_profile_host PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../../include)
  add_test(NAME trackball_profile_host COMMAND trackball_profile_host)
  return()
endif()

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(trackball_profile)

target_sources(app PRIVATE
  src/main.c
  ${CMAKE_CURRENT_LIST_DIR}/../../src/trackball_profile.c
)
target_include_directories(app PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../../include)
