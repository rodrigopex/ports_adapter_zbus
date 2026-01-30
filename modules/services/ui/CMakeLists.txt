if(CONFIG_UI_SERVICE)
    # Register proto for nanopb
    set_property(GLOBAL APPEND PROPERTY PROTO_FILES_LIST
        "${ZEPHYR_UI_SERVICE_MODULE_DIR}/ui_service.proto")

    # Auto-generate service infrastructure to build directory
    zephyr_service_generate(ui_service
        "${ZEPHYR_UI_SERVICE_MODULE_DIR}/ui_service.proto")

    # Expose build directory globally for service headers
    zephyr_include_directories("${CMAKE_CURRENT_BINARY_DIR}")

    # Interface library with include paths
    zephyr_interface_library_named(ui_service)
    target_include_directories(ui_service INTERFACE
        ${ZEPHYR_UI_SERVICE_MODULE_DIR}           # Source (for _impl.c)
        "${CMAKE_CURRENT_BINARY_DIR}"               # Build (for .h/.c/.priv.h)
        "${CMAKE_BINARY_DIR}/modules/services"      # Build parent (for .pb.h)
    )

    # Add sources: generated (build) + implementation (source)
    zephyr_library()
    zephyr_library_sources(
        ${ui_service_GENERATED_C}
        ${ZEPHYR_UI_SERVICE_MODULE_DIR}/ui_service_impl.c
    )

    add_dependencies(${ZEPHYR_CURRENT_LIBRARY} ui_service_codegen)
    zephyr_library_link_libraries(ui_service)
endif()
