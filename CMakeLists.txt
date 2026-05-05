cmake_minimum_required(VERSION 3.14)
project(NanoDB LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -pedantic -O2)
endif()

include_directories(${CMAKE_SOURCE_DIR}/include)

# ── Shared library of all engine modules ─────────────────────
set(ENGINE_SOURCES
    src/memory/BufferPool.cpp
    src/parser/QueryParser.cpp
)

add_library(nanodb_engine STATIC ${ENGINE_SOURCES})
target_include_directories(nanodb_engine PUBLIC ${CMAKE_SOURCE_DIR}/include)

# ── Main test runner executable ──────────────────────────────
add_executable(test_runner src/main.cpp)
target_link_libraries(test_runner PRIVATE nanodb_engine)

# ── Create data directory ─────────────────────────────────────
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/data)

# ── Custom target: run test suite ────────────────────────────
add_custom_target(run
    COMMAND ${CMAKE_BINARY_DIR}/test_runner
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running NanoDB Test Runner"
)
