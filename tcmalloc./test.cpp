#include "ConcurrentAlloc.hpp"
#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <cstring>

// 测试基本的分配和释放
void TestBasicAllocFree() {
    std::cout << "=== 测试基本分配和释放 ===" << std::endl;
    
    // 测试小对象分配
    void* p1 = ConcurrentAlloc(8);
    void* p2 = ConcurrentAlloc(16);
    void* p3 = ConcurrentAlloc(128);
    void* p4 = ConcurrentAlloc(1024);
    
    if (p1 && p2 && p3 && p4) {
        std::cout << "✓ 小对象分配成功" << std::endl;
        
        // 写入数据测试
        memset(p1, 0xAA, 8);
        memset(p2, 0xBB, 16);
        memset(p3, 0xCC, 128);
        memset(p4, 0xDD, 1024);
        
        std::cout << "✓ 数据写入成功" << std::endl;
        
        // 释放
        ConcurrentFree(p1);
        ConcurrentFree(p2);
        ConcurrentFree(p3);
        ConcurrentFree(p4);
        
        std::cout << "✓ 小对象释放成功" << std::endl;
    } else {
        std::cerr << "✗ 小对象分配失败" << std::endl;
    }
}

// 测试大对象分配（超过256KB）
void TestLargeAlloc() {
    std::cout << "\n=== 测试大对象分配 ===" << std::endl;
    
    void* p1 = ConcurrentAlloc(256 * 1024 + 1);
    void* p2 = ConcurrentAlloc(512 * 1024);
    
    if (p1 && p2) {
        std::cout << "✓ 大对象分配成功" << std::endl;
        
        // 写入数据测试
        memset(p1, 0xEE, 256 * 1024 + 1);
        memset(p2, 0xFF, 512 * 1024);
        
        std::cout << "✓ 大对象数据写入成功" << std::endl;
        
        ConcurrentFree(p1);
        ConcurrentFree(p2);
        
        std::cout << "✓ 大对象释放成功" << std::endl;
    } else {
        std::cerr << "✗ 大对象分配失败" << std::endl;
    }
}

// 测试边界情况
void TestEdgeCases() {
    std::cout << "\n=== 测试边界情况 ===" << std::endl;
    
    // 测试0字节分配
    void* p0 = ConcurrentAlloc(0);
    if (p0 == nullptr) {
        std::cout << "✓ 0字节分配返回nullptr" << std::endl;
    } else {
        std::cerr << "✗ 0字节分配应该返回nullptr" << std::endl;
        ConcurrentFree(p0);
    }
    
    // 测试nullptr释放
    ConcurrentFree(nullptr);
    std::cout << "✓ nullptr释放不崩溃" << std::endl;
    
    // 测试MAX_BYTES边界
    void* pMax = ConcurrentAlloc(MAX_BYTES);
    if (pMax) {
        memset(pMax, 0x55, MAX_BYTES);
        ConcurrentFree(pMax);
        std::cout << "✓ MAX_BYTES边界分配成功" << std::endl;
    } else {
        std::cerr << "✗ MAX_BYTES边界分配失败" << std::endl;
    }
}

// 多线程分配测试
void ThreadAllocTest(int threadId, int allocCount) {
    std::vector<void*> ptrs;
    ptrs.reserve(allocCount);
    
    // 分配
    for (int i = 0; i < allocCount; ++i) {
        size_t size = (i % 10 + 1) * 128; // 128 到 1280 字节
        void* p = ConcurrentAlloc(size);
        if (p) {
            memset(p, threadId & 0xFF, size);
            ptrs.push_back(p);
        }
    }
    
    // 释放一半
    for (size_t i = 0; i < ptrs.size() / 2; ++i) {
        ConcurrentFree(ptrs[i]);
    }
    
    // 再分配一些
    for (int i = 0; i < allocCount / 2; ++i) {
        size_t size = (i % 5 + 1) * 64;
        void* p = ConcurrentAlloc(size);
        if (p) {
            memset(p, (threadId + 1) & 0xFF, size);
            ptrs.push_back(p);
        }
    }
    
    // 释放剩余的
    for (size_t i = ptrs.size() / 2; i < ptrs.size(); ++i) {
        ConcurrentFree(ptrs[i]);
    }
}

void TestMultiThread() {
    std::cout << "\n=== 测试多线程并发分配 ===" << std::endl;
    
    const int threadCount = 8;
    const int allocPerThread = 10000;
    
    std::vector<std::thread> threads;
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < threadCount; ++i) {
        threads.emplace_back(ThreadAllocTest, i, allocPerThread);
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << "✓ " << threadCount << "个线程，每个线程" << allocPerThread 
              << "次分配，耗时: " << duration.count() << "ms" << std::endl;
}

// 压力测试
void StressTest() {
    std::cout << "\n=== 压力测试 ===" << std::endl;
    
    const int iterations = 10000;
    std::vector<void*> ptrs;
    
    for (int i = 0; i < iterations; ++i) {
        // 随机大小分配
        size_t size = (i % 100 + 1) * 16;
        void* p = ConcurrentAlloc(size);
        if (p) {
            memset(p, i & 0xFF, size);
            ptrs.push_back(p);
        }
        
        // 每100次分配后释放一些
        if (i % 100 == 0 && !ptrs.empty()) {
            size_t freeCount = ptrs.size() / 2;
            for (size_t j = 0; j < freeCount; ++j) {
                ConcurrentFree(ptrs[j]);
            }
            ptrs.erase(ptrs.begin(), ptrs.begin() + freeCount);
        }
    }
    
    // 释放所有剩余内存
    for (void* p : ptrs) {
        ConcurrentFree(p);
    }
    
    std::cout << "✓ 压力测试完成，分配/释放 " << iterations << " 次" << std::endl;
}

// 测试重复分配释放
void TestRepeatAllocFree() {
    std::cout << "\n=== 测试重复分配释放 ===" << std::endl;
    
    const int rounds = 1000;
    for (int i = 0; i < rounds; ++i) {
        void* p = ConcurrentAlloc(256);
        if (p) {
            memset(p, 0x77, 256);
            ConcurrentFree(p);
        }
    }
    
    std::cout << "✓ 重复分配释放 " << rounds << " 次成功" << std::endl;
}

int main() {
    std::cout << "开始测试高并发内存池..." << std::endl;
    std::cout << "PAGE_SHIFT: " << PAGE_SHIFT << std::endl;
    std::cout << "MAX_BYTES: " << MAX_BYTES << std::endl;
    std::cout << "NFREELIST: " << NFREELIST << std::endl;
    std::cout << "NPAGES: " << NPAGES << std::endl;
    std::cout << std::endl;
    
    try {
        TestBasicAllocFree();
        TestLargeAlloc();
        TestEdgeCases();
        TestRepeatAllocFree();
        StressTest();
        TestMultiThread();
        
        std::cout << "\n=== 所有测试完成 ===" << std::endl;
        std::cout << "如果没有段错误和异常，说明程序运行正常" << std::endl;
        std::cout << "请使用 valgrind 检测内存泄漏: valgrind --leak-check=full ./test" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "异常: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
