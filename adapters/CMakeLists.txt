if(CONFIG_ADAPTERS)
    zephyr_library()
    zephyr_library_sources("src/base_adapter.c")
    zephyr_library_sources_ifdef(CONFIG_TICK_TO_UI_ADAPTER "src/ZletTick+ZletUi_adapter.c")
    zephyr_library_sources_ifdef(CONFIG_BATTERY_TO_STORAGE_ADAPTER "src/ZletBattery+ZletStorage_adapter.c")
    zephyr_library_sources_ifdef(CONFIG_TICK_TO_STORAGE_ADAPTER "src/ZletTick+ZletStorage_adapter.c")
    zephyr_library_sources_ifdef(CONFIG_POSITION_TO_BATTERY_ADAPTER "src/ZletPosition+ZletBattery_adapter.c")

endif()
