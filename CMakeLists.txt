## Copyright 2009-2020 Intel Corporation
## SPDX-License-Identifier: Apache-2.0

cmake_dependent_option(
  OSPRAY_MODULE_WALL
  "Build the Display Wall TileOp module (requires dw2)"
  OFF
  OSPRAY_ENABLE_MODULES
  OFF
)

if (NOT OSPRAY_MODULE_WALL)
  return()
endif()

find_package(dw2_common REQUIRED)
find_package(dw2_client REQUIRED)

include(GenerateExportHeader)

add_library(ospray_module_wall SHARED ${OSPRAY_RESOURCE} DisplayWall.cpp)

generate_export_header(ospray_module_wall)

target_link_libraries(ospray_module_wall
PRIVATE
  ospray_module_ispc
  dw2_common::dw2_common
  dw2_client::dw2_client
)

target_include_directories(ospray_module_wall
PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}
)

ospray_install_library(ospray_module_wall lib)
