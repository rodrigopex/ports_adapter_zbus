set_property(
        GLOBAL
        APPEND
        PROPERTY
        PROTO_FILES_LIST
        "${ZEPHYR_SHARED_SERVICE_MODULE_DIR}/service.proto"
    )
message(WARNING "UI Service Proto File Added: ${ZEPHYR_UI_SERVICE_MODULE_DIR}/service.proto")

zephyr_include_directories(.)
zephyr_include_directories("${CMAKE_CURRENT_BINARY_DIR}/modules/services/shared")
