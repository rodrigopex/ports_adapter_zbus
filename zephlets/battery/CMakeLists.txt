if(CONFIG_BATTERY_ZEPHLET)
    # Register proto for nanopb
    set_property(GLOBAL APPEND PROPERTY PROTO_FILES_LIST
        "${ZEPHYR_BATTERY_ZEPHLET_MODULE_DIR}/battery_zephlet.proto")

    # Auto-generate zephlet infrastructure to build directory
    zephyr_zephlet_generate(battery_zephlet
        "${ZEPHYR_BATTERY_ZEPHLET_MODULE_DIR}/battery_zephlet.proto")

    # Expose build directory globally for zephlet headers
    zephyr_include_directories("${CMAKE_CURRENT_BINARY_DIR}")

    # Interface library with include paths
    zephyr_interface_library_named(battery_zephlet)
    target_include_directories(battery_zephlet INTERFACE
        ${ZEPHYR_BATTERY_ZEPHLET_MODULE_DIR}           # Source (for _impl.c)
        "${CMAKE_CURRENT_BINARY_DIR}"               # Build (for .h/.c/.priv.h)
        "${CMAKE_BINARY_DIR}/zephlets"              # Build parent (for .pb.h)
    )

    # Add sources: generated (build) + implementation (source)
    zephyr_library()
    zephyr_library_sources(
        ${battery_zephlet_GENERATED_C}
        ${ZEPHYR_BATTERY_ZEPHLET_MODULE_DIR}/battery_zephlet.c
    )

    add_dependencies(${ZEPHYR_CURRENT_LIBRARY} battery_zephlet_codegen)
    zephyr_library_link_libraries(battery_zephlet)
endif()
