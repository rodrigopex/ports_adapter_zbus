if(CONFIG_ZEPHLET_TAMPERING)
    zephyr_zephlet_generate(
        TYPE    tampering
        PREFIX  zlet_tampering
        SOURCES zlet_tampering.c)
endif()
