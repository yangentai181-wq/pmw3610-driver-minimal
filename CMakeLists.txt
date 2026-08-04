zephyr_library()

if(CONFIG_PMW3610)
  set(PMW3610_CPI_STEP 200)

  math(EXPR PMW3610_NORMAL_CPI_REMAINDER
    "${CONFIG_PMW3610_CPI} % ${PMW3610_CPI_STEP}")
  math(EXPR PMW3610_PRECISION_CPI_REMAINDER
    "${CONFIG_PMW3610_SNIPE_CPI} % ${PMW3610_CPI_STEP}")

  if(PMW3610_NORMAL_CPI_REMAINDER)
    message(FATAL_ERROR
      "CONFIG_PMW3610_CPI must be a multiple of ${PMW3610_CPI_STEP}")
  endif()

  if(PMW3610_PRECISION_CPI_REMAINDER)
    message(FATAL_ERROR
      "CONFIG_PMW3610_SNIPE_CPI must be a multiple of ${PMW3610_CPI_STEP}")
  endif()

  if(CONFIG_PMW3610_SNIPE_CPI GREATER CONFIG_PMW3610_CPI)
    message(FATAL_ERROR
      "CONFIG_PMW3610_SNIPE_CPI must not exceed CONFIG_PMW3610_CPI")
  endif()
endif()

zephyr_library_sources_ifdef(CONFIG_PMW3610 src/trackball_profile.c)
zephyr_library_sources_ifdef(CONFIG_PMW3610_TRACKBALL_SETTINGS src/trackball_settings.c)

if(CONFIG_PMW3610_STUDIO_RPC)
  list(APPEND CMAKE_MODULE_PATH ${ZEPHYR_BASE}/modules/nanopb)
  include(nanopb)
  set(NANOPB_GENERATE_CPP_STANDALONE OFF)
  set(NANOPB_GENERATE_CPP_APPEND_PATH TRUE)

  zephyr_nanopb_sources(app proto/zmk/trackball_settings/trackball_settings.proto)
  zephyr_library_include_directories(${CMAKE_CURRENT_BINARY_DIR})
  zephyr_library_sources_ifdef(CONFIG_PMW3610_STUDIO_RPC src/studio/trackball_settings_handler.c)
endif()

zephyr_library_sources_ifdef(CONFIG_PMW3610 src/pmw3610.c)
zephyr_library_sources_ifdef(CONFIG_PMW3610_DATA_LOGGER src/data_logger.c)
zephyr_library_sources_ifdef(CONFIG_PMW3610_DATA_LOGGER_BLE src/data_logger_ble.c)
zephyr_include_directories(${CMAKE_CURRENT_LIST_DIR}/include)
