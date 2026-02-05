# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
set(SERVICES_PATH "${CMAKE_SOURCE_DIR}/src/zephlets")

# Create a global property to hold the list of proto files across the modules
# Since we do not have the nanopb loaded in the external module addition time,
# we need to use a global property to be able to list the protobuf files.
set_property(GLOBAL PROPERTY PROTO_FILES_LIST)

set(EXTRA_ZEPHYR_MODULES
    "${CMAKE_SOURCE_DIR}/src/adapters"
    # shared_zephlet now loaded via west.yml
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

# Override CMAKE_CURRENT_SOURCE_DIR so nanopb generates files with correct directory structure
set(SAVED_CMAKE_CURRENT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(CMAKE_CURRENT_SOURCE_DIR "${CMAKE_SOURCE_DIR}/src/zephlets")
zephyr_nanopb_sources(app ${LOCAL_PROTO_FILES_LIST})
set(CMAKE_CURRENT_SOURCE_DIR ${SAVED_CMAKE_CURRENT_SOURCE_DIR})

include_directories(CMAKE_CURRENT_BINARY_DIR)
target_sources(app PRIVATE src/main.c)
