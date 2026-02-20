if(CONFIG_ADAPTERS)
    zephyr_library()
    zephyr_library_sources("src/base_adapter.c")

    set(ZEPHLETS_PATH "${CMAKE_SOURCE_DIR}/src/zephlets")

    zephlet_adapter_generate(ORIGIN tick DEST ui REPORTS events status)

endif()
