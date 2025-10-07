if (OPENZL_BUILD_PROTOBUF_TOOLS)
    cmake_minimum_required(VERSION 3.24)
    # Configure protobuf options before fetching
    set(protobuf_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(protobuf_WITH_ZLIB_DEFAULT OFF CACHE BOOL "" FORCE)
    set(protobuf_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

    # Get the protobuf library
    FetchContent_Declare(protobuf
        GIT_REPOSITORY https://github.com/protocolbuffers/protobuf.git
        GIT_TAG        v32.0
        FIND_PACKAGE_ARGS NAMES protobuf
    )
    FetchContent_MakeAvailable(protobuf)
    target_compile_options(libprotobuf PUBLIC
        -Wno-deprecated-declarations
        -Wno-stringop-overread
        -Wno-strict-aliasing
        -Wno-attributes
    )
    find_package(protobuf CONFIG REQUIRED)

    # Make the protobuf_serializer library
    file(GLOB protobuf_serializer_sources CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    )
    list(REMOVE_ITEM protobuf_serializer_sources "${CMAKE_CURRENT_SOURCE_DIR}/cli.cpp")

    add_library(protobuf_serializer ${protobuf_serializer_sources})

    target_link_libraries(protobuf_serializer PUBLIC
        openzl_cpp
        protobuf::libprotobuf
    )
    apply_openzl_compile_options_to_target(protobuf_serializer)

    # Make the protobuf_cli tool
    file(GLOB protobuf_cli_sources CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/cli.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/schema.proto")
    add_executable(protobuf_cli ${protobuf_cli_sources})

    include(FindProtobuf)
    protobuf_generate(TARGET protobuf_cli)

    target_link_libraries(protobuf_cli PRIVATE
        protobuf_serializer
        arg
        tools_io
        tools_training
    )
    target_include_directories(protobuf_cli PRIVATE ${PROJECT_BINARY_DIR})

    apply_openzl_compile_options_to_target(protobuf_cli)
endif()
