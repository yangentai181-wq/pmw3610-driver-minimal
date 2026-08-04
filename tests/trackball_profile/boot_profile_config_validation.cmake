if(DEFINED TRACKBALL_PROFILE_CONFIG_CASE)
  macro(zephyr_library)
  endmacro()

  macro(zephyr_library_sources_ifdef)
  endmacro()

  macro(zephyr_include_directories)
  endmacro()

  set(CONFIG_PMW3610 y)
  set(CONFIG_PMW3610_CPI "${TRACKBALL_PROFILE_NORMAL_CPI}")
  set(CONFIG_PMW3610_SNIPE_CPI "${TRACKBALL_PROFILE_PRECISION_CPI}")

  include("${TRACKBALL_PROFILE_MODULE_CMAKE}")
  return()
endif()

function(run_config_case label normal_cpi precision_cpi expect_success expected_error)
  execute_process(
    COMMAND "${CMAKE_COMMAND}"
      -DTRACKBALL_PROFILE_CONFIG_CASE=ON
      "-DTRACKBALL_PROFILE_MODULE_CMAKE=${TRACKBALL_PROFILE_MODULE_CMAKE}"
      "-DTRACKBALL_PROFILE_NORMAL_CPI=${normal_cpi}"
      "-DTRACKBALL_PROFILE_PRECISION_CPI=${precision_cpi}"
      -P "${CMAKE_CURRENT_LIST_FILE}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr
  )

  if(expect_success)
    if(NOT result EQUAL 0)
      message(FATAL_ERROR
        "${label}: valid boot profile was rejected:\n${stdout}${stderr}")
    endif()
    return()
  endif()

  if(result EQUAL 0)
    message(FATAL_ERROR
      "${label}: expected build-time boot-profile rejection, but module configuration succeeded")
  endif()

  string(FIND "${stdout}${stderr}" "${expected_error}" error_offset)
  if(error_offset EQUAL -1)
    message(FATAL_ERROR
      "${label}: expected '${expected_error}', got:\n${stdout}${stderr}")
  endif()
endfunction()

run_config_case(valid_minimum 200 200 TRUE "")
run_config_case(valid_maximum 3200 3200 TRUE "")
run_config_case(non_200_step 1650 200 FALSE
  "CONFIG_PMW3610_CPI must be a multiple of 200")
run_config_case(precision_non_200_step 800 250 FALSE
  "CONFIG_PMW3610_SNIPE_CPI must be a multiple of 200")
run_config_case(precision_above_normal 200 400 FALSE
  "CONFIG_PMW3610_SNIPE_CPI must not exceed CONFIG_PMW3610_CPI")
