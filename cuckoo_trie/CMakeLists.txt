cmake_minimum_required(VERSION 3.8)
project(cuckoo_trie)

include_directories(src)
set(CMAKE_CXX_STANDARD 17)

set(CT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set (CT_SOURCES ${CT_DIR}/main.c ${CT_DIR}/util.c ${CT_DIR}/verify_trie.c ${CT_DIR}/random.c ${CT_DIR}/atomics.c ${CT_DIR}/mt_debug.c)
set(CT_DEPS ${CT_SOURCES} ${CT_DIR}/atomics.h ${CT_DIR}/config.h ${CT_DIR}/cuckoo_trie.h ${CT_DIR}/main.h ${CT_DIR}/util.h
        ${CT_DIR}/key_object.h ${CT_DIR}/cuckoo_trie_internal.h ${CT_DIR}/random.h ${CT_DIR}/mt_debug.h)
set(CT_TEST_SOURCES ${CT_DIR}/test.c ${CT_DIR}/random.c ${CT_DIR}/dataset.c ${CT_DIR}/util.c)
set(CT_TEST_DEPS ${CT_TEST_SOURCES} ${CT_DIR}/cuckoo_trie.h ${CT_DIR}/random.h ${CT_DIR}/key_object.h ${CT_DIR}/dataset.h ${CT_DIR}/util.h)
add_library(cuckoo_trie SHARED ${CT_DEPS})
add_executable(ct_test ${CT_TEST_DEPS})
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(ct_test PRIVATE cuckoo_trie Threads::Threads)

