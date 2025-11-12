#include "ConcurrentAlloc.hpp"
#include <chrono>
#include <iostream>
#include <vector>
#include <thread>
#include <cstring>

struct Result {
    double alloc_ms;
    double free_ms;
};

Result bench_tcmalloc(size_t n, size_t sz) {
    std::vector<void*> ptrs;
    ptrs.reserve(n);

    auto t1 = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < n; ++i) {
        void* p = ConcurrentAlloc(sz);
        ptrs.push_back(p);
    }
    auto t2 = std::chrono::high_resolution_clock::now();
    for (void* p : ptrs) {
        ConcurrentFree(p);
    }
    auto t3 = std::chrono::high_resolution_clock::now();

    double alloc_ms = std::chrono::duration<double, std::milli>(t2 - t1).count();
    double free_ms  = std::chrono::duration<double, std::milli>(t3 - t2).count();
    return {alloc_ms, free_ms};
}

Result bench_new(size_t n, size_t sz) {
    std::vector<char*> ptrs;
    ptrs.reserve(n);

    auto t1 = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < n; ++i) {
        char* p = new char[sz];
        ptrs.push_back(p);
    }
    auto t2 = std::chrono::high_resolution_clock::now();
    for (char* p : ptrs) {
        delete[] p;
    }
    auto t3 = std::chrono::high_resolution_clock::now();

    double alloc_ms = std::chrono::duration<double, std::milli>(t2 - t1).count();
    double free_ms  = std::chrono::duration<double, std::milli>(t3 - t2).count();
    return {alloc_ms, free_ms};
}

void single_thread_test(size_t n, const std::vector<size_t>& sizes) {
    std::cout << "\n===== 单线程性能测试 每种大小分配 " << n << " 次 =====\n";
    for (size_t sz : sizes) {
        auto r1 = bench_tcmalloc(n, sz);
        auto r2 = bench_new(n, sz);
        std::cout << "大小 " << sz << " 字节:\n";
        std::cout << "  tcmalloc  alloc: " << r1.alloc_ms << " ms, free: " << r1.free_ms << " ms\n";
        std::cout << "  new/delete alloc: " << r2.alloc_ms << " ms, free: " << r2.free_ms << " ms\n";
    }
}

void thread_worker(size_t n, size_t sz) {
    std::vector<void*> ptrs;
    ptrs.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        void* p = ConcurrentAlloc(sz);
        ptrs.push_back(p);
    }
    for (void* p : ptrs) ConcurrentFree(p);
}

void multi_thread_test(size_t threads, size_t n_per_thread, size_t sz) {
    auto start = std::chrono::high_resolution_clock::now();
    std::vector<std::thread> ts;
    for (size_t i = 0; i < threads; ++i) ts.emplace_back(thread_worker, n_per_thread, sz);
    for (auto& t : ts) t.join();
    auto end = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(end - start).count();
    std::cout << "\n===== 多线程 tcmalloc 测试 size=" << sz << " threads=" << threads << " allocs/thread=" << n_per_thread << " =====\n";
    std::cout << "总耗时 " << ms << " ms\n";
}

int main(int argc, char** argv) {
    size_t n = 1'000'000;
    if (argc > 1) n = std::stoull(argv[1]);

    std::vector<size_t> sizes = {8, 16, 64, 256, 1024, 4096};
    single_thread_test(n, sizes);

    multi_thread_test(8, n / 8, 64);
    return 0;
}
