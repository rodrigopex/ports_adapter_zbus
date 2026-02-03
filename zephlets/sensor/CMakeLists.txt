if(CONFIG_ZEPHLET_SENSOR)
    # Register proto for nanopb
    set_property(GLOBAL APPEND PROPERTY PROTO_FILES_LIST
        "${ZEPHYR_ZLET_SENSOR_MODULE_DIR}/zlet_sensor.proto")

    # Auto-generate zephlet infrastructure to build directory
    zephyr_zephlet_generate(zlet_sensor
        "${ZEPHYR_ZLET_SENSOR_MODULE_DIR}/zlet_sensor.proto")

    # Expose build directory globally for zephlet headers
    zephyr_include_directories("${CMAKE_CURRENT_BINARY_DIR}")

    # Interface library with include paths
    zephyr_interface_library_named(zlet_sensor)
    target_include_directories(zlet_sensor INTERFACE
        ${ZEPHYR_ZLET_SENSOR_MODULE_DIR}           # Source (for _impl.c)
        "${CMAKE_CURRENT_BINARY_DIR}"               # Build (for .h/.c/.priv.h)
        "${CMAKE_BINARY_DIR}/zephlets"              # Build parent (for .pb.h)
    )

    # Add sources: generated (build) + implementation (source)
    zephyr_library()
    zephyr_library_sources(
        ${zlet_sensor_GENERATED_C}
        ${ZEPHYR_ZLET_SENSOR_MODULE_DIR}/zlet_sensor.c
    )

    add_dependencies(${ZEPHYR_CURRENT_LIBRARY} zlet_sensor_codegen)
    zephyr_library_link_libraries(zlet_sensor)
endif()
