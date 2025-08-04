cmake_minimum_required(VERSION 3.10)
project(perplexity_mcp C)

set(CMAKE_C_STANDARD 11)

# Include directories
include_directories(
        include
        src
        src/models
)

# Find required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(CURL REQUIRED libcurl)
pkg_check_modules(CJSON REQUIRED libcjson)

# Source files
set(SOURCES
        src/main.c
        src/http_client.c
        src/json_utils.c
        src/mcp_protocol.c
        src/models/async_models.c
        src/models/model_router.c
        src/models/sync_models.c
        src/usage.c
)

# Create executable
add_executable(perplexity_mcp ${SOURCES})

# Define _GNU_SOURCE to enable GNU extensions
target_compile_definitions(perplexity_mcp PRIVATE _GNU_SOURCE)

# Link libraries
target_link_libraries(perplexity_mcp
        ${CURL_LIBRARIES}
        ${CJSON_LIBRARIES}
)

# Include directories for libraries
target_include_directories(perplexity_mcp PRIVATE
        ${CURL_INCLUDE_DIRS}
        ${CJSON_INCLUDE_DIRS}
)

# Compiler flags
target_compile_options(perplexity_mcp PRIVATE
        ${CURL_CFLAGS_OTHER}
        ${CJSON_CFLAGS_OTHER}
)

# Set output name to match your Makefile
set_target_properties(perplexity_mcp PROPERTIES
        OUTPUT_NAME perplexity-mcp-server
)
