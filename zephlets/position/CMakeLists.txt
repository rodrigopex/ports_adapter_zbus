if(CONFIG_ZEPHLET_POSITION)
    # Register proto for nanopb
    set_property(GLOBAL APPEND PROPERTY PROTO_FILES_LIST
        "${ZEPHYR_ZLET_POSITION_MODULE_DIR}/zlet_position.proto")

    # Auto-generate zephlet infrastructure to build directory
    zephyr_zephlet_generate(zlet_position
        "${ZEPHYR_ZLET_POSITION_MODULE_DIR}/zlet_position.proto")

    # Expose build directory globally for zephlet headers
    zephyr_include_directories("${CMAKE_CURRENT_BINARY_DIR}")

    # Interface library with include paths
    zephyr_interface_library_named(zlet_position)
    target_include_directories(position_zephlet INTERFACE
        ${ZEPHYR_ZLET_POSITION_MODULE_DIR}           # Source (for .c)
        "${CMAKE_CURRENT_BINARY_DIR}"               # Build (for .h/.c/.priv.h)
        "${CMAKE_BINARY_DIR}/zephlets"              # Build parent (for .pb.h)
    )

    # Add sources: generated (build) + implementation (source)
    zephyr_library()
    zephyr_library_sources(
        ${zlet_position_GENERATED_C}
        ${ZEPHYR_ZLET_POSITION_MODULE_DIR}/zlet_position.c
    )

    add_dependencies(${ZEPHYR_CURRENT_LIBRARY} zlet_position_codegen)
    zephyr_library_link_libraries(position_zephlet)
endif()
