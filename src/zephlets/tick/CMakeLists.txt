if(CONFIG_ZEPHLET_TICK)
    # Register hand-written proto file for nanopb.
    set_property(GLOBAL APPEND PROPERTY PROTO_FILES_LIST
        "${ZEPHYR_ZLET_TICK_MODULE_DIR}/zlet_tick.proto")

    # Phase 2: codegen is bypassed. The hand-written oracle lives at
    # docs/v03_codegen_oracle/zlet_tick/ (see that README). Copy it to
    # the build dir — the same path that Phase 3 codegen will emit to —
    # so the rest of the build treats the interface files as pure build
    # artifacts.
    get_filename_component(APP_REPO_ROOT
        "${ZEPHYR_ZLET_TICK_MODULE_DIR}/../../.." ABSOLUTE)
    set(ORACLE_DIR "${APP_REPO_ROOT}/docs/v03_codegen_oracle/zlet_tick")
    set(GENERATED_DIR "${CMAKE_BINARY_DIR}/modules/zlet_tick")

    file(MAKE_DIRECTORY ${GENERATED_DIR})
    configure_file(
        "${ORACLE_DIR}/zlet_tick_interface.h"
        "${GENERATED_DIR}/zlet_tick_interface.h"
        COPYONLY)
    configure_file(
        "${ORACLE_DIR}/zlet_tick_interface.c"
        "${GENERATED_DIR}/zlet_tick_interface.c"
        COPYONLY)

    # Expose the build-dir location for the interface header and the
    # nanopb output parent directory.
    zephyr_include_directories(
        ${GENERATED_DIR}
        ${CMAKE_BINARY_DIR}
    )

    # Compile the hand-written user impl (source tree) + generated
    # interface (build tree).
    zephyr_library()
    zephyr_library_sources(
        ${ZEPHYR_ZLET_TICK_MODULE_DIR}/zlet_tick.c
        ${GENERATED_DIR}/zlet_tick_interface.c
    )
endif()
