cmake_minimum_required(VERSION 3.20)

# Must be set before project()
set(CMAKE_OSX_ARCHITECTURES "arm64")

project(minidb CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wextra -Werror=switch)

# ---- PostgreSQL client (libpq) ------------------------------------------
# Override with: cmake -S . -B build -DPG_ROOT=$(brew --prefix libpq)
set(PG_ROOT "/opt/homebrew/opt/postgresql@15" CACHE PATH "libpq install prefix")
set(PG_INCLUDE_DIR "${PG_ROOT}/include")
set(PG_LIB_DIR     "${PG_ROOT}/lib")

if(EXISTS "${PG_INCLUDE_DIR}/libpq-fe.h")
    set(HAVE_PG ON)
    link_directories(${PG_LIB_DIR})
else()
    set(HAVE_PG OFF)
    message(WARNING "libpq not found at ${PG_ROOT}; skipping targets that need it "
                    "(validator and the engine tests). Set -DPG_ROOT=... to enable them.")
endif()

include_directories(
    engine/include
    validator/include
    server/include
    session/include
    parser/include
    catalog/include
    ${PG_INCLUDE_DIR}
)

# ---- libraries ----------------------------------------------------------
add_library(minidb_engine STATIC
    engine/src/page.cpp
    engine/src/tuple.cpp
    engine/src/pagestore.cpp
    engine/src/transaction_manager.cpp
    engine/src/buffer.cpp
    engine/src/free_space_map.cpp
    engine/src/visibility_map.cpp
    engine/src/lock_manager.cpp
    engine/src/thread_manager.cpp
    server/src/server.cpp
)
target_include_directories(minidb_engine PUBLIC parser/include)

add_library(minidb_session STATIC
    session/src/session_manager.cpp
    session/src/session.cpp
    session/src/pg_stat_activity.cpp
)

add_library(minidb_parser STATIC
    parser/src/lexer.cpp
    parser/src/parser.cpp
    parser/src/printer.cpp
)

# ---- parser tests (no Postgres needed) ----------------------------------
add_executable(ast_test       tests/parser_test/ast_test.cpp)
add_executable(tokenizer_test tests/parser_test/lexer_test.cpp)
add_executable(parse_test     tests/parser_test/parse_test.cpp)

target_link_libraries(ast_test       minidb_parser)
target_link_libraries(tokenizer_test minidb_parser)
target_link_libraries(parse_test     minidb_parser)

# run_tests (regression suite) is added here once the harness is written

# ---- targets that need libpq --------------------------------------------
if(HAVE_PG)
    add_executable(validator validator/src/main.cpp)
    target_link_libraries(validator minidb_engine minidb_parser pq)

    add_executable(heap_test            tests/heap_test.cpp)
    add_executable(txn_test             tests/txn_test.cpp)
    add_executable(tuple_visiblity_test tests/tuple_visiblity_test.cpp)
    add_executable(buffer_test          tests/buffer_test.cpp)
    add_executable(insert_tuple_test    tests/insert_tuple_test.cpp)
    add_executable(lock_manager_test    tests/lock_manager_test.cpp)

    target_link_libraries(heap_test            minidb_engine pq)
    target_link_libraries(txn_test             minidb_engine pq)
    target_link_libraries(tuple_visiblity_test minidb_engine pq)
    target_link_libraries(buffer_test          minidb_engine pq)
    target_link_libraries(insert_tuple_test    minidb_engine pq)
    target_link_libraries(lock_manager_test    minidb_engine pq)
endif()