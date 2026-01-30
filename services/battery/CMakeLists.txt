if(CONFIG_BATTERY_SERVICE)
    # Register proto for nanopb
    set_property(GLOBAL APPEND PROPERTY PROTO_FILES_LIST
        "${ZEPHYR_BATTERY_SERVICE_MODULE_DIR}/battery_service.proto")

    # Auto-generate service infrastructure to build directory
    zephyr_service_generate(battery_service
        "${ZEPHYR_BATTERY_SERVICE_MODULE_DIR}/battery_service.proto")

    # Expose build directory globally for service headers
    zephyr_include_directories("${CMAKE_CURRENT_BINARY_DIR}")

    # Interface library with include paths
    zephyr_interface_library_named(battery_service)
    target_include_directories(battery_service INTERFACE
        ${ZEPHYR_BATTERY_SERVICE_MODULE_DIR}           # Source (for _impl.c)
        "${CMAKE_CURRENT_BINARY_DIR}"               # Build (for .h/.c/.priv.h)
        "${CMAKE_BINARY_DIR}/services"              # Build parent (for .pb.h)
    )

    # Add sources: generated (build) + implementation (source)
    zephyr_library()
    zephyr_library_sources(
        ${battery_service_GENERATED_C}
        ${ZEPHYR_BATTERY_SERVICE_MODULE_DIR}/battery_service_impl.c
    )

    add_dependencies(${ZEPHYR_CURRENT_LIBRARY} battery_service_codegen)
    zephyr_library_link_libraries(battery_service)
endif()
