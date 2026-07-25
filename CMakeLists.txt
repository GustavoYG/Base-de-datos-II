cmake_minimum_required(VERSION 3.16)
project(BasesDatosII CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

file(GLOB_RECURSE SOURCES
    src/*.cpp
)

# Excluir el test de la lista de fuentes del ejecutable principal
list(FILTER SOURCES EXCLUDE REGEX ".*test_.*\\.cpp")

add_executable(sgdb ${SOURCES})

# Test executable
add_executable(test_all
    tests/test_all_features.cpp
    src/common/schema.cpp
    src/common/utils.cpp
    src/db/catalog.cpp
    src/index/bplus_tree.cpp
    src/index/index_manager.cpp
    src/io/csv_loader.cpp
    src/io/csv_reader.cpp
    src/io/interactive_cli.cpp
    src/io/schema_io.cpp
    src/io/serializer.cpp
    src/io/type_infer.cpp
    src/sql/benchmark.cpp
    src/sql/operators.cpp
    src/sql/sql_completer.cpp
    src/sql/sql_engine.cpp
    src/storage/buffer_pool.cpp
    src/storage/page_manager.cpp
    src/storage/record_manager.cpp
    src/storage/table.cpp
    src/storage/wal_log.cpp
)
