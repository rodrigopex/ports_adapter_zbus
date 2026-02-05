if(CONFIG_ADAPTERS)
    zephyr_library()
    zephyr_library_sources("src/base_adapter.c")
    zephyr_library_sources_ifdef(CONFIG_TICK_TO_UI_ADAPTER "src/Tick+Ui_zlet_adapter.c")
    zephyr_library_sources_ifdef(CONFIG_BATTERY_TO_STORAGE_ADAPTER "src/Battery+Storage_zlet_adapter.c")
    zephyr_library_sources_ifdef(CONFIG_TICK_TO_STORAGE_ADAPTER "src/Tick+Storage_zlet_adapter.c")
    zephyr_library_sources_ifdef(CONFIG_POSITION_TO_BATTERY_ADAPTER "src/Position+Battery_zlet_adapter.c")

endif()
