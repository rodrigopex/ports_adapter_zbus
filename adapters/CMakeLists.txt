if(CONFIG_ADAPTERS)
    zephyr_library()
    zephyr_library_sources("base_adapter.c")
    zephyr_library_sources_ifdef(CONFIG_TICK_TO_UI_ADAPTER "TickService+UiService_adapter.c")
    zephyr_library_sources_ifdef(CONFIG_BATTERY_TO_STORAGE_ADAPTER "BatteryService+StorageService_adapter.c")
    zephyr_library_sources_ifdef(CONFIG_TICK_TO_STORAGE_ADAPTER "TickService+StorageService_adapter.c")

endif()
