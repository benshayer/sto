cmake_minimum_required(VERSION 3.8)
project(cuckoo_trie)

include_directories(src)
set(CMAKE_CXX_STANDARD 11)
set (CT_SOURCES src/main.c src/util.c src/verify_trie.c src/random.c src/atomics.c src/mt_debug.c)
set(CT_DEPS ${CT_SOURCES} src/atomics.h src/config.h src/cuckoo_trie.h src/main.h src/util.h
src/key_object.h src/cuckoo_trie_internal.h src/random.h src/mt_debug.h)

add_library(cuckoo_trie ${CT_DEPS})
