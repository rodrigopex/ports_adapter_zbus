if(CONFIG_BATTERY_SERVICE)
    # Add the proto files to the global PROTO_FILES_LIST property
    set_property(
        GLOBAL
        APPEND
        PROPERTY
        PROTO_FILES_LIST
        "${ZEPHYR_BATTERY_SERVICE_MODULE_DIR}/battery_service.proto"
    )
    message(WARNING "Battery Service Proto File Added: ${ZEPHYR_BATTERY_SERVICE_MODULE_DIR}/battery_service.proto")

    zephyr_interface_library_named(battery_service)

    target_include_directories(
        battery_service
        INTERFACE
        ${ZEPHYR_BATTERY_SERVICE_MODULE_DIR}
        "${CMAKE_CURRENT_BINARY_DIR}/modules/services/battery"
    )

    zephyr_library()

    zephyr_library_sources(battery_service.c)

    zephyr_library_link_libraries(battery_service)

    zephyr_include_directories(.)
endif()
