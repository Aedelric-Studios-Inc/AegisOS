cmake_minimum_required(VERSION 3.20)
project(AegisOS LANGUAGES C ASM)

add_custom_target(aegisos-all
    COMMAND ${CMAKE_MAKE_PROGRAM} all
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Building AegisOS via the repository Makefile")

add_custom_target(aegisos-kernel
    COMMAND ${CMAKE_MAKE_PROGRAM} kernel
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Building the AegisOS kernel via the repository Makefile")

add_custom_target(aegisos-rust
    COMMAND ${CMAKE_MAKE_PROGRAM} rust
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Building Rust components via the repository Makefile")
