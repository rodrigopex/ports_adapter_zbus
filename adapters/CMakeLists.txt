if(CONFIG_ADAPTERS)
    zephyr_library()
    zephyr_library_sources_ifdef(CONFIG_TICK_TO_UI_ADAPTER "tick>ui+adapter.c")
    zephyr_library_sources_ifdef(CONFIG_DEBUG_ADAPTER "debug_adapter.c")

endif()
