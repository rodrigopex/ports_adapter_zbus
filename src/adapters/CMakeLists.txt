if(CONFIG_ADAPTERS)
    zephyr_library()
    zephyr_library_sources("src/base_adapter.c")
    zephyr_library_sources_ifdef(CONFIG_TICK_TO_UI_ADAPTER "src/Tick+Ui_zlet_adapter.c")

endif()
