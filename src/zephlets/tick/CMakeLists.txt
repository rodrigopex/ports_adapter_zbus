if(CONFIG_ZEPHLET_TICK)
    # Generate full proto from schema + base proto → build dir
    zephyr_zephlet_generate_proto(zlet_tick
        "${ZEPHYR_SHARED_ZEPHLET_MODULE_DIR}/zephlet.proto"
        "${ZEPHYR_ZLET_TICK_MODULE_DIR}/zlet_tick.proto")

    # Register generated proto for nanopb
    set_property(GLOBAL APPEND PROPERTY PROTO_FILES_LIST
        "${zlet_tick_GENERATED_PROTO}")

    # Auto-generate zephlet infrastructure from generated proto
    zephyr_zephlet_generate(zlet_tick "${zlet_tick_GENERATED_PROTO}")
    add_dependencies(zlet_tick_codegen zlet_tick_proto_gen)

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
