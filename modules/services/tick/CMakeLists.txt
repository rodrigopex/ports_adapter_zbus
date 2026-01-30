if(CONFIG_TICK_SERVICE)
    # Register proto for nanopb
    set_property(GLOBAL APPEND PROPERTY PROTO_FILES_LIST
        "${ZEPHYR_TICK_SERVICE_MODULE_DIR}/tick_service.proto")

    # Auto-generate service infrastructure to build directory
    zephyr_service_generate(tick_service
        "${ZEPHYR_TICK_SERVICE_MODULE_DIR}/tick_service.proto")

    # Expose build directory globally for service headers
    zephyr_include_directories("${CMAKE_CURRENT_BINARY_DIR}")

    # Interface library with include paths
    zephyr_interface_library_named(tick_service)
    target_include_directories(tick_service INTERFACE
        ${ZEPHYR_TICK_SERVICE_MODULE_DIR}           # Source (for _impl.c)
        "${CMAKE_CURRENT_BINARY_DIR}"               # Build (for .h/.c/.priv.h)
        "${CMAKE_BINARY_DIR}/modules/services"      # Build parent (for .pb.h)
    )

    # Add sources: generated (build) + implementation (source)
    zephyr_library()
    zephyr_library_sources(
        ${tick_service_GENERATED_C}
        ${ZEPHYR_TICK_SERVICE_MODULE_DIR}/tick_service_impl.c
    )

    add_dependencies(${ZEPHYR_CURRENT_LIBRARY} tick_service_codegen)
    zephyr_library_link_libraries(tick_service)
endif()
