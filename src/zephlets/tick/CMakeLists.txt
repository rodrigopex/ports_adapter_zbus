if(CONFIG_ZEPHLET_TICK)
    # Register proto for nanopb
    set_property(GLOBAL APPEND PROPERTY PROTO_FILES_LIST
        "${ZEPHYR_ZLET_TICK_MODULE_DIR}/zlet_tick.proto")

    # Auto-generate zephlet infrastructure to build directory
    zephyr_zephlet_generate(zlet_tick
        "${ZEPHYR_ZLET_TICK_MODULE_DIR}/zlet_tick.proto")

    # Expose build directory globally for zephlet headers
    zephyr_include_directories("${CMAKE_CURRENT_BINARY_DIR}")

    # Interface library with include paths
    zephyr_interface_library_named(zlet_tick)
    target_include_directories(zlet_tick INTERFACE
        ${ZEPHYR_ZLET_TICK_MODULE_DIR}           # Source (for _impl.c)
        "${CMAKE_CURRENT_BINARY_DIR}"               # Build (for .h/.c/.priv.h)
        "${CMAKE_BINARY_DIR}/zephlets"              # Build parent (for .pb.h)
    )

    # Add sources: generated (build) + implementation (source)
    zephyr_library()
    zephyr_library_sources(
        ${zlet_tick_GENERATED_C}
        ${ZEPHYR_ZLET_TICK_MODULE_DIR}/zlet_tick.c
    )

    add_dependencies(${ZEPHYR_CURRENT_LIBRARY} zlet_tick_codegen)
    zephyr_library_link_libraries(zlet_tick)
endif()
