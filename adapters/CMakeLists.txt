if(CONFIG_ADAPTERS)
    zephyr_library()
    zephyr_library_sources("src/base_adapter.c")
    zephyr_library_sources_ifdef(CONFIG_TICK_TO_UI_ADAPTER "src/TickService+UiService_adapter.c")
    zephyr_library_sources_ifdef(CONFIG_BATTERY_TO_STORAGE_ADAPTER "src/BatteryService+StorageService_adapter.c")
    zephyr_library_sources_ifdef(CONFIG_TICK_TO_STORAGE_ADAPTER "src/TickService+StorageService_adapter.c")

endif()
