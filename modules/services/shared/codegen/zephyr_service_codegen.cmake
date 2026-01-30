# SPDX-License-Identifier: Apache-2.0
# Service code generation CMake module
# Auto-generates service infrastructure (.h, .c, private/*_priv.h) during build

function(zephyr_service_generate SERVICE_NAME PROTO_FILE)
  set(CODEGEN_SCRIPT "${CMAKE_SOURCE_DIR}/modules/services/shared/codegen/generate_service.py")
  set(OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}")

  set(GENERATED_H "${OUTPUT_DIR}/${SERVICE_NAME}.h")
  set(GENERATED_C "${OUTPUT_DIR}/${SERVICE_NAME}.c")
  set(GENERATED_PRIV_H "${OUTPUT_DIR}/private/${SERVICE_NAME}_priv.h")

  file(GLOB TEMPLATE_FILES "${CMAKE_SOURCE_DIR}/modules/services/shared/codegen/templates/*.jinja")
  file(MAKE_DIRECTORY "${OUTPUT_DIR}/private")

  # Check if _impl.c exists in source, generate once if missing (bootstrap)
  set(IMPL_FILE "${CMAKE_CURRENT_SOURCE_DIR}/${SERVICE_NAME}_impl.c")
  if(NOT EXISTS ${IMPL_FILE})
    message(STATUS "Bootstrapping ${SERVICE_NAME}_impl.c (one-time generation)")
    execute_process(
      COMMAND ${PYTHON_EXECUTABLE} ${CODEGEN_SCRIPT}
        --proto ${PROTO_FILE}
        --output-dir ${CMAKE_CURRENT_SOURCE_DIR}
        --service-name ${SERVICE_NAME}
        --module-dir ${CMAKE_CURRENT_SOURCE_DIR}
        --impl-only
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
      RESULT_VARIABLE BOOTSTRAP_RESULT
    )
    if(NOT BOOTSTRAP_RESULT EQUAL 0)
      message(FATAL_ERROR "Failed to bootstrap ${SERVICE_NAME}_impl.c")
    endif()
  endif()

  # Auto-generate infrastructure files to build directory
  add_custom_command(
    OUTPUT ${GENERATED_H} ${GENERATED_C} ${GENERATED_PRIV_H}
    COMMAND ${PYTHON_EXECUTABLE} ${CODEGEN_SCRIPT}
      --proto ${PROTO_FILE}
      --output-dir ${OUTPUT_DIR}
      --service-name ${SERVICE_NAME}
      --module-dir ${CMAKE_CURRENT_SOURCE_DIR}
      --no-generate-impl
    DEPENDS ${PROTO_FILE} ${TEMPLATE_FILES} ${CODEGEN_SCRIPT}
    COMMENT "Generating ${SERVICE_NAME} from ${PROTO_FILE}"
    VERBATIM
  )

  add_custom_target(${SERVICE_NAME}_codegen
    DEPENDS ${GENERATED_H} ${GENERATED_C} ${GENERATED_PRIV_H})

  set(${SERVICE_NAME}_GENERATED_C ${GENERATED_C} PARENT_SCOPE)
endfunction()
