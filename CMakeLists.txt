# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

# Global property holding the list of proto files contributed by each
# zephlet module (registered in their own CMakeLists.txt).
set_property(GLOBAL PROPERTY PROTO_FILES_LIST)

# Zephlets live directly under src/. The framework imposes no layout;
# this app's choice is a flat src/<name>/ per zephlet.
set(EXTRA_ZEPHYR_MODULES
    "${CMAKE_SOURCE_DIR}/src/ui"
    "${CMAKE_SOURCE_DIR}/src/tick"
    "${CMAKE_SOURCE_DIR}/src/tampering"
)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(ports_adapters_zbus)

list(APPEND CMAKE_MODULE_PATH ${ZEPHYR_BASE}/modules/nanopb)
include(nanopb)

set(NANOPB_OPTIONS "--c-style")

get_property(LOCAL_PROTO_FILES_LIST GLOBAL PROPERTY PROTO_FILES_LIST)

# Anchor nanopb output at src/ so each proto lands at
# ${CMAKE_BINARY_DIR}/<zephlet>/<prefix>.pb.h — matching the
# `#include "<type>/<prefix>.pb.h"` pattern the generator emits.
set(SAVED_CMAKE_CURRENT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(CMAKE_CURRENT_SOURCE_DIR "${CMAKE_SOURCE_DIR}/src")
zephyr_nanopb_sources(app ${LOCAL_PROTO_FILES_LIST})
set(CMAKE_CURRENT_SOURCE_DIR ${SAVED_CMAKE_CURRENT_SOURCE_DIR})

target_sources(app PRIVATE src/main.c src/policies.c)
