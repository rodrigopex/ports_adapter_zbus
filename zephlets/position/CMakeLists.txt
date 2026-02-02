if(CONFIG_POSITION_ZEPHLET)
    # Register proto for nanopb
    set_property(GLOBAL APPEND PROPERTY PROTO_FILES_LIST
        "${ZEPHYR_POSITION_ZEPHLET_MODULE_DIR}/position_zephlet.proto")

    # Auto-generate zephlet infrastructure to build directory
    zephyr_zephlet_generate(position_zephlet
        "${ZEPHYR_POSITION_ZEPHLET_MODULE_DIR}/position_zephlet.proto")

    # Expose build directory globally for zephlet headers
    zephyr_include_directories("${CMAKE_CURRENT_BINARY_DIR}")

    # Interface library with include paths
    zephyr_interface_library_named(position_zephlet)
    target_include_directories(position_zephlet INTERFACE
        ${ZEPHYR_POSITION_ZEPHLET_MODULE_DIR}           # Source (for .c)
        "${CMAKE_CURRENT_BINARY_DIR}"               # Build (for .h/.c/.priv.h)
        "${CMAKE_BINARY_DIR}/zephlets"              # Build parent (for .pb.h)
    )

    # Add sources: generated (build) + implementation (source)
    zephyr_library()
    zephyr_library_sources(
        ${position_zephlet_GENERATED_C}
        ${ZEPHYR_POSITION_ZEPHLET_MODULE_DIR}/position_zephlet.c
    )

    add_dependencies(${ZEPHYR_CURRENT_LIBRARY} position_zephlet_codegen)
    zephyr_library_link_libraries(position_zephlet)
endif()
