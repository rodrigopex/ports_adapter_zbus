if(CONFIG_ZEPHLET_TICK)
    # Register hand-written proto file for nanopb (consumed by the app
    # CMakeLists via PROTO_FILES_LIST). Codegen is disabled for tick
    # during Phase 2; the generator will reproduce this wiring in Phase 3.
    set_property(GLOBAL APPEND PROPERTY PROTO_FILES_LIST
        "${ZEPHYR_ZLET_TICK_MODULE_DIR}/zlet_tick.proto")

    # Expose tick's source directory (for zlet_tick_interface.h) and the
    # nanopb output parent directory (for "tick/zlet_tick.pb.h") globally
    # so the test / app target can include them.
    zephyr_include_directories(
        ${ZEPHYR_ZLET_TICK_MODULE_DIR}
        ${CMAKE_BINARY_DIR}
    )

    # Compile the hand-written interface + implementation.
    zephyr_library()
    zephyr_library_sources(
        ${ZEPHYR_ZLET_TICK_MODULE_DIR}/zlet_tick.c
        ${ZEPHYR_ZLET_TICK_MODULE_DIR}/zlet_tick_interface.c
    )
endif()
