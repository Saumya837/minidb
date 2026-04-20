set(CMAKE_OSX_ARCHITECTURES "arm64")
cmake_minimum_required(VERSION 3.20)
project(minidb CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(PG_INCLUDE_DIR "/opt/homebrew/opt/postgresql@15/include")
set(PG_LIB_DIR     "/opt/homebrew/opt/postgresql@15/lib")

include_directories(
    engine/include
    validator/include
    ${PG_INCLUDE_DIR}
)

link_directories(${PG_LIB_DIR})

add_library(minidb_engine STATIC
    engine/src/page.cpp
    engine/src/tuple.cpp
    engine/src/pagestore.cpp
)

add_executable(validator
    validator/src/main.cpp
)
target_link_libraries(validator minidb_engine pq)

add_executable(heap_test
    tests/heap_test.cpp
)
target_link_libraries(heap_test minidb_engine pq)