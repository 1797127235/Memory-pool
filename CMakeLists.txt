cmake_minimum_required(VERSION 4.0)
project(Memory_pool)

set(CMAKE_CXX_STANDARD 14)

include_directories(.)
include_directories(tcmalloc)

add_executable(Memory_pool
        tcmalloc/ThreadCache.h
        tcmalloc/ThreadCache.h
        tcmalloc/Common.h
        tcmalloc/ThreadCache.cpp
        tcmalloc/CentralCache.cpp
        tcmalloc/CentralCache.h
        tcmalloc/ConcurrentAlloc.h)
