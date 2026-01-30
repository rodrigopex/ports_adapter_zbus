# Service codegen setup - loaded first so all services can use it
list(APPEND CMAKE_MODULE_PATH "${ZEPHYR_SHARED_SERVICE_MODULE_DIR}/codegen")
include(zephyr_service_codegen)

# Find Python for service code generation
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(PYTHON_EXECUTABLE ${Python3_EXECUTABLE})

# Check for required Python packages
execute_process(
    COMMAND ${PYTHON_EXECUTABLE} -c "import proto_schema_parser; import jinja2"
    RESULT_VARIABLE PKG_CHECK
    OUTPUT_QUIET ERROR_QUIET
)
if(NOT PKG_CHECK EQUAL 0)
    message(FATAL_ERROR "Missing Python packages. Install: pip install -r ${ZEPHYR_SHARED_SERVICE_MODULE_DIR}/codegen/requirements.txt")
endif()

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

    # Expose nanopb parent directory globally (all .pb.h files)
    zephyr_include_directories("${CMAKE_BINARY_DIR}/services")
    # Expose shared service build directory for service.pb.h
    zephyr_include_directories("${CMAKE_BINARY_DIR}/services/shared")

endif()
