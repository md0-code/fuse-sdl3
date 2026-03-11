if(NOT DEFINED OUTPUT_FILE)
  message(FATAL_ERROR "OUTPUT_FILE is required")
endif()

if(NOT DEFINED SHELL_EXECUTABLE)
  message(FATAL_ERROR "SHELL_EXECUTABLE is required")
endif()

if(NOT DEFINED SHELL_ARG)
  message(FATAL_ERROR "SHELL_ARG is required")
endif()

if(NOT DEFINED COMMAND_STRING AND NOT DEFINED COMMAND_FILE)
  message(FATAL_ERROR "COMMAND_STRING or COMMAND_FILE is required")
endif()

if(DEFINED COMMAND_FILE)
  string(REGEX REPLACE "^\"(.*)\"$" "\\1" COMMAND_FILE "${COMMAND_FILE}")
  file(READ "${COMMAND_FILE}" COMMAND_STRING)
  string(STRIP "${COMMAND_STRING}" COMMAND_STRING)
endif()

set(_output_tmp "${OUTPUT_FILE}.tmp")
set(_shell_is_cmd FALSE)

if(WIN32 AND SHELL_EXECUTABLE MATCHES "(^|[/\\\\])cmd(\\\\.exe)?$")
  set(_shell_is_cmd TRUE)
endif()

if(_shell_is_cmd)
  set(_command_invocation "\"${COMMAND_STRING}\"")
  execute_process(
    COMMAND "${SHELL_EXECUTABLE}" "/D" "/S" "${SHELL_ARG}" "${_command_invocation}"
    RESULT_VARIABLE _result
    OUTPUT_FILE "${_output_tmp}"
    ERROR_VARIABLE _stderr
  )
else()
  execute_process(
    COMMAND "${SHELL_EXECUTABLE}" "${SHELL_ARG}" "${COMMAND_STRING}"
    RESULT_VARIABLE _result
    OUTPUT_FILE "${_output_tmp}"
    ERROR_VARIABLE _stderr
  )
endif()

if(NOT _result EQUAL 0)
  file(REMOVE "${_output_tmp}")
  string(STRIP "${_stderr}" _stderr)
  message(FATAL_ERROR "Command failed with exit code ${_result}: ${COMMAND_STRING}\n${_stderr}")
endif()

execute_process(COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${_output_tmp}" "${OUTPUT_FILE}")
file(REMOVE "${_output_tmp}")
