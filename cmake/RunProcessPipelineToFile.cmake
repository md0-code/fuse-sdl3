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

execute_process(
  COMMAND ${COMMAND1}
  COMMAND ${COMMAND2}
  RESULTS_VARIABLE _results
  OUTPUT_FILE "${_output_tmp}"
  ERROR_VARIABLE _stderr
)

list(LENGTH _results _result_count)
if(_result_count LESS 2)
  file(REMOVE "${_output_tmp}")
  string(REPLACE ";" " " _command1_text "${COMMAND1}")
  string(REPLACE ";" " " _command2_text "${COMMAND2}")
  string(STRIP "${_stderr}" _stderr)
  message(FATAL_ERROR "Pipeline did not return both command results: ${_command1_text} | ${_command2_text}\n${_stderr}")
endif()

list(GET _results 0 _result1)
list(GET _results 1 _result2)

if(NOT _result1 EQUAL 0)
  file(REMOVE "${_output_tmp}")
  string(REPLACE ";" " " _command1_text "${COMMAND1}")
  string(STRIP "${_stderr}" _stderr)
  message(FATAL_ERROR "Command failed with exit code ${_result1}: ${_command1_text}\n${_stderr}")
endif()

if(NOT _result2 EQUAL 0)
  file(REMOVE "${_output_tmp}")
  string(REPLACE ";" " " _command2_text "${COMMAND2}")
  string(STRIP "${_stderr}" _stderr)
  message(FATAL_ERROR "Command failed with exit code ${_result2}: ${_command2_text}\n${_stderr}")
endif()

execute_process(COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${_output_tmp}" "${OUTPUT_FILE}")
file(REMOVE "${_output_tmp}")
