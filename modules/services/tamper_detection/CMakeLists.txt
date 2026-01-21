if(CONFIG_TAMPER_DETECTION_SERVICE)
    # Add the proto files to the global PROTO_FILES_LIST property
    set_property(
        GLOBAL
        APPEND
        PROPERTY
        PROTO_FILES_LIST
        "${ZEPHYR_TAMPER_DETECTION_SERVICE_MODULE_DIR}/tamper_detection_service.proto"
    )
    message(WARNING "Tamper Detection Service Proto File Added: ${ZEPHYR_TAMPER_DETECTION_SERVICE_MODULE_DIR}/tamper_detection_service.proto")

    zephyr_interface_library_named(tamper_detection_service)

    target_include_directories(
        tamper_detection_service
        INTERFACE
        ${ZEPHYR_TAMPER_DETECTION_SERVICE_MODULE_DIR}
        "${CMAKE_CURRENT_BINARY_DIR}/modules/services/tamper_detection"
    )

    zephyr_library()

    zephyr_library_sources(tamper_detection_service.c)

    zephyr_library_link_libraries(tamper_detection_service)

    zephyr_include_directories(.)
endif()
