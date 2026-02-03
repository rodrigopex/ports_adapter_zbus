# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
set(SERVICES_PATH "${CMAKE_SOURCE_DIR}/zephlets")

# Create a global property to hold the list of proto files across the modules
# Since we do not have the nanopb loaded in the external module addition time,
# we need to use a global property to be able to list the protobuf files.
set_property(GLOBAL PROPERTY PROTO_FILES_LIST)

set(EXTRA_ZEPHYR_MODULES
    "${CMAKE_SOURCE_DIR}/adapters"
    "${SERVICES_PATH}/shared"
    "${SERVICES_PATH}/ui"
    "${SERVICES_PATH}/tick"
    "${SERVICES_PATH}/battery"
    "${SERVICES_PATH}/storage"
    "${SERVICES_PATH}/position"
)
message(WARNING "Extra Zephyr Modules Path: ${EXTRA_ZEPHYR_MODULES}")


find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(hello_world)

list(APPEND CMAKE_MODULE_PATH ${ZEPHYR_BASE}/modules/nanopb)
include(nanopb)

set(NANOPB_OPTIONS "--c-style")

# Retrieve the list of proto files from the global property
get_property(LOCAL_PROTO_FILES_LIST GLOBAL PROPERTY PROTO_FILES_LIST)

zephyr_nanopb_sources(app ${LOCAL_PROTO_FILES_LIST})

include_directories(CMAKE_CURRENT_BINARY_DIR)
target_sources(app PRIVATE src/main.c)
