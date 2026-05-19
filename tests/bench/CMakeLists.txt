# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

set_property(GLOBAL PROPERTY PROTO_FILES_LIST)

# Pull in the production tick zephlet unchanged. Path is relative to
# this CMakeLists, then resolved to absolute.
get_filename_component(_app_root "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
set(EXTRA_ZEPHYR_MODULES "${_app_root}/src/tick")

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(zephlet_bench)

list(APPEND CMAKE_MODULE_PATH ${ZEPHYR_BASE}/modules/nanopb)
include(nanopb)

set(NANOPB_OPTIONS "--c-style")

get_property(LOCAL_PROTO_FILES_LIST GLOBAL PROPERTY PROTO_FILES_LIST)

set(SAVED_CMAKE_CURRENT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(CMAKE_CURRENT_SOURCE_DIR "${_app_root}/src")
zephyr_nanopb_sources(app ${LOCAL_PROTO_FILES_LIST})
set(CMAKE_CURRENT_SOURCE_DIR ${SAVED_CMAKE_CURRENT_SOURCE_DIR})

target_sources(app PRIVATE src/main.c)
