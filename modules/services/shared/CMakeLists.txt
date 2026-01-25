if(CONFIG_SERVICES)
    set_property(
        GLOBAL
        APPEND
        PROPERTY
        PROTO_FILES_LIST
        "${ZEPHYR_SHARED_SERVICE_MODULE_DIR}/service.proto"
    )
    message(WARNING "UI Service Proto File Added: ${ZEPHYR_UI_SERVICE_MODULE_DIR}/service.proto")

    zephyr_library()
    zephyr_library_sources("${ZEPHYR_SHARED_SERVICE_MODULE_DIR}/service.c")
    zephyr_include_directories(.)
    zephyr_linker_sources(DATA_SECTIONS service_iterables.ld)

endif()
