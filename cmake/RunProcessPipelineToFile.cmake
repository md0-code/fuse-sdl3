if(NOT DEFINED OUTPUT_FILE)
  message(FATAL_ERROR "OUTPUT_FILE is required")
endif()

if(NOT DEFINED COMMAND1_FILE)
  message(FATAL_ERROR "COMMAND1_FILE is required")
endif()

if(NOT DEFINED COMMAND2_FILE)
  message(FATAL_ERROR "COMMAND2_FILE is required")
endif()

file(STRINGS "${COMMAND1_FILE}" COMMAND1)
file(STRINGS "${COMMAND2_FILE}" COMMAND2)

if(NOT COMMAND1)
  message(FATAL_ERROR "COMMAND1_FILE did not contain a command")
endif()

if(NOT COMMAND2)
  message(FATAL_ERROR "COMMAND2_FILE did not contain a command")
endif()

set(_output_tmp "${OUTPUT_FILE}.tmp")
set(_input_tmp "${OUTPUT_FILE}.input.tmp")

execute_process(
  COMMAND ${COMMAND1}
  RESULT_VARIABLE _result1
  OUTPUT_VARIABLE _stdout1
  ERROR_VARIABLE _stderr1
)

if(NOT _result1 EQUAL 0)
  string(REPLACE ";" " " _command1_text "${COMMAND1}")
  string(STRIP "${_stderr1}" _stderr1)
  message(FATAL_ERROR "Command failed with exit code ${_result1}: ${_command1_text}\n${_stderr1}")
endif()

file(WRITE "${_input_tmp}" "${_stdout1}")

execute_process(
  COMMAND ${COMMAND2}
  INPUT_FILE "${_input_tmp}"
  RESULT_VARIABLE _result2
  OUTPUT_FILE "${_output_tmp}"
  ERROR_VARIABLE _stderr2
)

file(REMOVE "${_input_tmp}")

if(NOT _result2 EQUAL 0)
  file(REMOVE "${_output_tmp}")
  string(REPLACE ";" " " _command2_text "${COMMAND2}")
  string(STRIP "${_stderr2}" _stderr2)
  message(FATAL_ERROR "Command failed with exit code ${_result2}: ${_command2_text}\n${_stderr2}")
endif()

execute_process(COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${_output_tmp}" "${OUTPUT_FILE}")
file(REMOVE "${_output_tmp}")
