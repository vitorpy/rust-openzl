# Copyright (c) Meta Platforms, Inc. and affiliates.

set(OPENZL_BUILD_ARG_TOOLS OFF)

if(OPENZL_BUILD_TOOLS)
  set(OPENZL_BUILD_ARG_TOOLS ON)
endif()
if(OPENZL_BUILD_CLI)
  set(OPENZL_BUILD_ARG_TOOLS ON)
endif()

if (OPENZL_BUILD_ARG_TOOLS)
    file(
        GLOB_RECURSE arg_sources
        CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
    file(
        GLOB_RECURSE arg_headers
        CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/*.h")

    add_library(arg
      ${arg_sources}
    )
    target_include_directories(arg PUBLIC ${PROJECT_SOURCE_DIR})
    apply_openzl_compile_options_to_target(arg)
endif()
