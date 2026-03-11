if(NOT DEFINED OUTPUT_FILE)
  message(FATAL_ERROR "OUTPUT_FILE is required")
endif()

if(NOT DEFINED COMMAND_FILE)
  message(FATAL_ERROR "COMMAND_FILE is required")
endif()

file(STRINGS "${COMMAND_FILE}" COMMAND)
if(NOT COMMAND)
  message(FATAL_ERROR "COMMAND_FILE did not contain a command")
endif()

set(_output_tmp "${OUTPUT_FILE}.tmp")

execute_process(
  COMMAND ${COMMAND}
  RESULT_VARIABLE _result
  OUTPUT_FILE "${_output_tmp}"
  ERROR_VARIABLE _stderr
)

if(NOT _result EQUAL 0)
  file(REMOVE "${_output_tmp}")
  string(REPLACE ";" " " _command_text "${COMMAND}")
  string(STRIP "${_stderr}" _stderr)
  message(FATAL_ERROR "Command failed with exit code ${_result}: ${_command_text}\n${_stderr}")
endif()

execute_process(COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${_output_tmp}" "${OUTPUT_FILE}")
file(REMOVE "${_output_tmp}")
