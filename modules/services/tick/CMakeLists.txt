if(CONFIG_TICK_SERVICE)
    # Add the proto files to the global PROTO_FILES_LIST property
    set_property(
        GLOBAL
        APPEND
        PROPERTY
        PROTO_FILES_LIST
        "${ZEPHYR_TICK_SERVICE_MODULE_DIR}/tick_service.proto"
    )
    message(WARNING "Tick Service Proto File Added: ${ZEPHYR_TICK_SERVICE_MODULE_DIR}/tick_service.proto")

    zephyr_interface_library_named(tick_service)

    target_include_directories(
        tick_service
        INTERFACE
        ${ZEPHYR_UI_SERVICE_MODULE_DIR}
        "${CMAKE_CURRENT_BINARY_DIR}/modules/services/ui"
    )

    zephyr_library()

    zephyr_library_sources(tick_service.c)

    zephyr_library_link_libraries(tick_service)

    zephyr_include_directories(.)
endif()
