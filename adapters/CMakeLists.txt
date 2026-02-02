if(CONFIG_ADAPTERS)
    zephyr_library()
    zephyr_library_sources("src/base_adapter.c")
    zephyr_library_sources_ifdef(CONFIG_TICK_TO_UI_ADAPTER "src/TickZephlet+UiZephlet_adapter.c")
    zephyr_library_sources_ifdef(CONFIG_BATTERY_TO_STORAGE_ADAPTER "src/BatteryZephlet+StorageZephlet_adapter.c")
    zephyr_library_sources_ifdef(CONFIG_TICK_TO_STORAGE_ADAPTER "src/TickZephlet+StorageZephlet_adapter.c")

endif()
