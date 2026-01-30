if(CONFIG_STORAGE_SERVICE)
    # Register proto for nanopb
    set_property(GLOBAL APPEND PROPERTY PROTO_FILES_LIST
        "${ZEPHYR_STORAGE_SERVICE_MODULE_DIR}/storage_service.proto")

    # Auto-generate service infrastructure to build directory
    zephyr_service_generate(storage_service
        "${ZEPHYR_STORAGE_SERVICE_MODULE_DIR}/storage_service.proto")

    # Expose build directory globally for service headers
    zephyr_include_directories("${CMAKE_CURRENT_BINARY_DIR}")

    # Interface library with include paths
    zephyr_interface_library_named(storage_service)
    target_include_directories(storage_service INTERFACE
        ${ZEPHYR_STORAGE_SERVICE_MODULE_DIR}           # Source (for _impl.c)
        "${CMAKE_CURRENT_BINARY_DIR}"               # Build (for .h/.c/.priv.h)
        "${CMAKE_BINARY_DIR}/modules/services"      # Build parent (for .pb.h)
    )

    # Add sources: generated (build) + implementation (source)
    zephyr_library()
    zephyr_library_sources(
        ${storage_service_GENERATED_C}
        ${ZEPHYR_STORAGE_SERVICE_MODULE_DIR}/storage_service_impl.c
    )

    add_dependencies(${ZEPHYR_CURRENT_LIBRARY} storage_service_codegen)
    zephyr_library_link_libraries(storage_service)
endif()
