if(CONFIG_ZEPHLET_UI)
    # Generate full proto from schema + base proto → build dir
    zephyr_zephlet_generate_proto(zlet_ui
        "${ZEPHYR_SHARED_ZEPHLET_MODULE_DIR}/zephlet.proto"
        "${ZEPHYR_ZLET_UI_MODULE_DIR}/zlet_ui.proto")

    # Register generated proto for nanopb
    set_property(GLOBAL APPEND PROPERTY PROTO_FILES_LIST
        "${zlet_ui_GENERATED_PROTO}")

    # Auto-generate zephlet infrastructure from generated proto
    zephyr_zephlet_generate(zlet_ui "${zlet_ui_GENERATED_PROTO}")
    add_dependencies(zlet_ui_codegen zlet_ui_proto_gen)

    # Expose build directory globally for zephlet headers
    zephyr_include_directories("${CMAKE_CURRENT_BINARY_DIR}")

    # Interface library with include paths
    zephyr_interface_library_named(zlet_ui)
    target_include_directories(zlet_ui INTERFACE
        ${ZEPHYR_ZLET_UI_MODULE_DIR}           # Source (for _impl.c)
        "${CMAKE_CURRENT_BINARY_DIR}"               # Build (for .h/.c/.priv.h)
        "${CMAKE_BINARY_DIR}/zephlets"              # Build parent (for .pb.h)
    )

    # Add sources: generated (build) + implementation (source)
    zephyr_library()
    zephyr_library_sources(
        ${zlet_ui_GENERATED_C}
        ${ZEPHYR_ZLET_UI_MODULE_DIR}/zlet_ui.c
    )

    add_dependencies(${ZEPHYR_CURRENT_LIBRARY} zlet_ui_codegen)
    zephyr_library_link_libraries(zlet_ui)
endif()
