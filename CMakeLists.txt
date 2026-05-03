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
    engine/src/transaction_manager.cpp
    engine/src/buffer_pool.cpp 
    engine/src/free_space_map.cpp
    engine/src/visibility_map.cpp
)

add_executable(validator
    validator/src/main.cpp
)
target_link_libraries(validator minidb_engine pq)

add_executable(heap_test
    tests/heap_test.cpp
)
add_executable(txn_test
    tests/txn_test.cpp
)

add_executable(tuple_visiblity_test
    tests/tuple_visiblity_test.cpp
)

add_executable(buffer_pool_test
    tests/buffer_pool_test.cpp        
)

add_executable(insert_tuple_test
    tests/insert_tuple_test.cpp        
)


target_link_libraries(heap_test minidb_engine pq)
target_link_libraries(txn_test minidb_engine pq)
target_link_libraries(tuple_visiblity_test minidb_engine pq)
target_link_libraries(buffer_pool_test minidb_engine pq)
target_link_libraries(insert_tuple_test minidb_engine pq)
