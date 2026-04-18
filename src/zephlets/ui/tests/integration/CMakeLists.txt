cmake_minimum_required(VERSION 3.20.0)

get_filename_component(ZEPHLET_DIR    "${CMAKE_CURRENT_SOURCE_DIR}/../.." REALPATH)
get_filename_component(ZEPHLET_PARENT "${ZEPHLET_DIR}/.."                  REALPATH)

set_property(GLOBAL PROPERTY PROTO_FILES_LIST)

list(APPEND EXTRA_ZEPHYR_MODULES "${ZEPHLET_DIR}")

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(test_ui_integration)

list(APPEND CMAKE_MODULE_PATH ${ZEPHYR_BASE}/modules/nanopb)
include(nanopb)
set(NANOPB_OPTIONS "--c-style")
get_property(LOCAL_PROTO_FILES_LIST GLOBAL PROPERTY PROTO_FILES_LIST)

set(SAVED_CMAKE_CURRENT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(CMAKE_CURRENT_SOURCE_DIR "${ZEPHLET_PARENT}")
zephyr_nanopb_sources(app ${LOCAL_PROTO_FILES_LIST})
set(CMAKE_CURRENT_SOURCE_DIR ${SAVED_CMAKE_CURRENT_SOURCE_DIR})

target_sources(app PRIVATE
    src/test_ui_integration.c
)

target_include_directories(app PRIVATE
    ${ZEPHYR_BASE}/subsys/testsuite/include
)
