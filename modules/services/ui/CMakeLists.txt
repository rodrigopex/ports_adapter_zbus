if(CONFIG_UI_SERVICE)
    # Add the proto files to the global PROTO_FILES_LIST property
    set_property(
        GLOBAL
        APPEND
        PROPERTY
        PROTO_FILES_LIST
        "${ZEPHYR_UI_SERVICE_MODULE_DIR}/ui_service.proto"
    )
    message(WARNING "UI Service Proto File Added: ${ZEPHYR_UI_SERVICE_MODULE_DIR}/ui_service.proto")

    zephyr_interface_library_named(ui_service)

    target_include_directories(
        ui_service
        INTERFACE
        ${ZEPHYR_UI_SERVICE_MODULE_DIR}
        "${CMAKE_CURRENT_BINARY_DIR}/modules/services/ui"
    )

    zephyr_library()

    zephyr_library_sources(ui_service.c)

    zephyr_library_link_libraries(ui_service)

    zephyr_include_directories(.)
endif()
