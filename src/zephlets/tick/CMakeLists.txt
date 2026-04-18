if(CONFIG_ZEPHLET_TICK)
    # Register hand-written proto for nanopb (consumed by the app/test
    # CMakeLists via PROTO_FILES_LIST).
    set_property(GLOBAL APPEND PROPERTY PROTO_FILES_LIST
        "${ZEPHYR_ZLET_TICK_MODULE_DIR}/zlet_tick.proto")

    # v0.3 codegen: parse the proto's service block and emit
    # zlet_tick_interface.{h,c} into the build dir.
    set(GENERATED_DIR "${CMAKE_BINARY_DIR}/modules/zlet_tick")
    set(GENERATED_H "${GENERATED_DIR}/zlet_tick_interface.h")
    set(GENERATED_C "${GENERATED_DIR}/zlet_tick_interface.c")
    set(CODEGEN_SCRIPT
        "${ZEPHYR_ZEPHLET_MODULE_DIR}/codegen/generate_zephlet.py")
    file(GLOB TEMPLATE_FILES
        "${ZEPHYR_ZEPHLET_MODULE_DIR}/codegen/templates/zephlet_interface*.jinja")

    file(MAKE_DIRECTORY ${GENERATED_DIR})
    add_custom_command(
        OUTPUT ${GENERATED_H} ${GENERATED_C}
        COMMAND ${PYTHON_EXECUTABLE} ${CODEGEN_SCRIPT}
            --proto ${ZEPHYR_ZLET_TICK_MODULE_DIR}/zlet_tick.proto
            --output-dir ${GENERATED_DIR}
            --type tick
            --prefix zlet_tick
        DEPENDS
            ${ZEPHYR_ZLET_TICK_MODULE_DIR}/zlet_tick.proto
            ${CODEGEN_SCRIPT}
            ${TEMPLATE_FILES}
        COMMENT "Generating zlet_tick interface (v0.3)"
        VERBATIM)
    add_custom_target(zlet_tick_codegen DEPENDS ${GENERATED_H} ${GENERATED_C})

    # Expose user source dir (zlet_tick.h), generated dir (interface.h),
    # and binary root (nanopb .pb.h).
    zephyr_include_directories(
        ${ZEPHYR_ZLET_TICK_MODULE_DIR}
        ${GENERATED_DIR}
        ${CMAKE_BINARY_DIR})

    zephyr_library()
    zephyr_library_sources(
        ${ZEPHYR_ZLET_TICK_MODULE_DIR}/zlet_tick.c
        ${GENERATED_C})
    add_dependencies(${ZEPHYR_CURRENT_LIBRARY} zlet_tick_codegen)
endif()
