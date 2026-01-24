if(CONFIG_ADAPTERS)
    zephyr_library()
    zephyr_library_sources("base_adapter.c")
    zephyr_library_sources_ifdef(CONFIG_TICK_TO_UI_ADAPTER "TickService+UiService_adapter.c")

endif()
