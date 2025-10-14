#include"ConcurrentAlloc.h"
#include<vector>
#include<thread>
#include<chrono>

// 测试基本的申请和释放
void TestBasicAllocFree()
{
    cout << "=== 测试基本申请和释放 ===" << endl;
    
    std::vector<size_t> sizes = {8, 16, 64, 128, 512, 1024, 4*1024, 16*1024, 64*1024};
    
    for(auto size : sizes)
    {
        void* ptr = ConcurrentAlloc(size);
        if(ptr != nullptr)
        {
            cout << "申请 " << size << " 字节,地址: " << ptr;
            memset(ptr, 0xAA, size);
            
            ConcurrentFree(ptr);
            cout << " -> 已释放" << endl;
        }
    }
    cout << endl;
}

// 测试批量申请后批量释放
void TestBatchAllocFree()
{
    cout << "=== 测试批量申请后批量释放 ===" << endl;
    
    const int count = 100;
    std::vector<void*> ptrs;
    
    // 申请
    cout << "申请 " << count << " 个 128 字节的内存块..." << endl;
    for(int i = 0; i < count; ++i)
    {
        void* ptr = ConcurrentAlloc(128);
        if(ptr != nullptr)
        {
            ptrs.push_back(ptr);
            memset(ptr, i % 256, 128);
        }
    }
    cout << "成功申请 " << ptrs.size() << " 个内存块" << endl;
    
    // 释放
    cout << "开始释放内存..." << endl;
    for(auto ptr : ptrs)
    {
        ConcurrentFree(ptr);
    }
    cout << "成功释放 " << ptrs.size() << " 个内存块" << endl;
    cout << endl;
}

// 测试交替申请和释放
void TestInterleavedAllocFree()
{
    cout << "=== 测试交替申请和释放 ===" << endl;
    
    const int rounds = 50;
    std::vector<void*> ptrs;
    
    for(int i = 0; i < rounds; ++i)
    {
        // 申请3个
        for(int j = 0; j < 3; ++j)
        {
            void* ptr = ConcurrentAlloc(256);
            if(ptr != nullptr)
            {
                ptrs.push_back(ptr);
                memset(ptr, 0xBB, 256);
            }
        }
        
        // 释放2个
        if(ptrs.size() >= 2)
        {
            ConcurrentFree(ptrs.back());
            ptrs.pop_back();
            ConcurrentFree(ptrs.back());
            ptrs.pop_back();
        }
    }
    
    cout << "交替操作完成,剩余 " << ptrs.size() << " 个未释放的内存块" << endl;
    
    // 释放剩余的
    for(auto ptr : ptrs)
    {
        ConcurrentFree(ptr);
    }
    cout << "已释放所有剩余内存块" << endl;
    cout << endl;
}

// 测试内存重用 - 申请释放再申请
void TestMemoryReuse()
{
    cout << "=== 测试内存重用 ===" << endl;
    
    std::vector<void*> firstBatch;
    std::vector<void*> secondBatch;
    
    // 第一批申请
    cout << "第一批: 申请 50 个 512 字节的内存块..." << endl;
    for(int i = 0; i < 50; ++i)
    {
        void* ptr = ConcurrentAlloc(512);
        if(ptr != nullptr)
        {
            firstBatch.push_back(ptr);
            memset(ptr, 0xCC, 512);
        }
    }
    cout << "第一批申请完成: " << firstBatch.size() << " 个" << endl;
    
    // 释放第一批
    cout << "释放第一批..." << endl;
    for(auto ptr : firstBatch)
    {
        ConcurrentFree(ptr);
    }
    
    // 第二批申请(应该重用刚释放的内存)
    cout << "第二批: 申请 50 个 512 字节的内存块..." << endl;
    for(int i = 0; i < 50; ++i)
    {
        void* ptr = ConcurrentAlloc(512);
        if(ptr != nullptr)
        {
            secondBatch.push_back(ptr);
            memset(ptr, 0xDD, 512);
        }
    }
    cout << "第二批申请完成: " << secondBatch.size() << " 个" << endl;
    
    // 检查是否有地址重用
    int reuseCount = 0;
    for(auto p1 : firstBatch)
    {
        for(auto p2 : secondBatch)
        {
            if(p1 == p2)
            {
                reuseCount++;
                break;
            }
        }
    }
    cout << "内存重用次数: " << reuseCount << " (预期应该有重用)" << endl;
    
    // 释放第二批
    for(auto ptr : secondBatch)
    {
        ConcurrentFree(ptr);
    }
    cout << endl;
}

// 测试不同大小混合申请释放
void TestMixedSizeAllocFree()
{
    cout << "=== 测试不同大小混合申请释放 ===" << endl;
    
    std::vector<std::pair<void*, size_t>> allocations;
    std::vector<size_t> sizes = {8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096};
    
    // 混合申请
    cout << "混合申请不同大小的内存..." << endl;
    for(int i = 0; i < 100; ++i)
    {
        size_t size = sizes[i % sizes.size()];
        void* ptr = ConcurrentAlloc(size);
        if(ptr != nullptr)
        {
            allocations.push_back({ptr, size});
            memset(ptr, 0xEE, size);
        }
    }
    cout << "成功申请 " << allocations.size() << " 个不同大小的内存块" << endl;
    
    // 混合释放
    cout << "开始释放..." << endl;
    for(auto& alloc : allocations)
    {
        ConcurrentFree(alloc.first);
    }
    cout << "成功释放所有内存块" << endl;
    cout << endl;
}

// 多线程测试函数
void ThreadAllocFreeTest(int threadId, int iterations)
{
    cout << "线程 " << threadId << " 开始..." << endl;
    
    std::vector<void*> ptrs;
    
    for(int i = 0; i < iterations; ++i)
    {
        // 申请一些内存
        for(int j = 0; j < 10; ++j)
        {
            size_t size = ((i + j) % 10 + 1) * 128;
            void* ptr = ConcurrentAlloc(size);
            if(ptr != nullptr)
            {
                ptrs.push_back(ptr);
                memset(ptr, threadId, size);
            }
        }
        
        // 释放一半
        size_t toFree = ptrs.size() / 2;
        for(size_t k = 0; k < toFree; ++k)
        {
            if(!ptrs.empty())
            {
                ConcurrentFree(ptrs.back());
                ptrs.pop_back();
            }
        }
    }
    
    // 释放剩余的
    for(auto ptr : ptrs)
    {
        ConcurrentFree(ptr);
    }
    
    cout << "线程 " << threadId << " 完成" << endl;
}

// 测试多线程并发申请释放
void TestMultiThreadAllocFree()
{
    cout << "=== 测试多线程并发申请释放 ===" << endl;
    
    const int threadCount = 4;
    const int iterationsPerThread = 20;
    
    std::vector<std::thread> threads;
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    for(int i = 0; i < threadCount; ++i)
    {
        threads.emplace_back(ThreadAllocFreeTest, i, iterationsPerThread);
    }
    
    for(auto& t : threads)
    {
        t.join();
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    cout << "所有线程完成,耗时: " << duration.count() << " 毫秒" << endl;
    cout << endl;
}

// 压力测试 - 大量申请释放循环
void TestStressAllocFree()
{
    cout << "=== 压力测试: 大量申请释放循环 ===" << endl;
    
    const int cycles = 5;
    const int allocsPerCycle = 200;
    
    for(int cycle = 0; cycle < cycles; ++cycle)
    {
        cout << "第 " << (cycle + 1) << " 轮..." << endl;
        
        std::vector<void*> ptrs;
        
        // 申请
        for(int i = 0; i < allocsPerCycle; ++i)
        {
            size_t size = (i * 137 % 200) * 128 + 8;
            if(size > MAX_BYTES) size = MAX_BYTES;
            
            void* ptr = ConcurrentAlloc(size);
            if(ptr != nullptr)
            {
                ptrs.push_back(ptr);
                if(size >= 4)
                {
                    *(int*)ptr = i;
                }
            }
        }
        
        cout << "  申请了 " << ptrs.size() << " 个内存块" << endl;
        
        // 释放
        for(auto ptr : ptrs)
        {
            ConcurrentFree(ptr);
        }
        
        cout << "  释放了 " << ptrs.size() << " 个内存块" << endl;
    }
    
    cout << "压力测试完成!" << endl;
    cout << endl;
}

// 测试边界情况
void TestEdgeCases()
{
    cout << "=== 测试边界情况 ===" << endl;
    
    // 最小大小
    cout << "测试最小大小 (1 字节):" << endl;
    void* ptr1 = ConcurrentAlloc(1);
    if(ptr1)
    {
        cout << "  申请成功,地址: " << ptr1 << endl;
        ConcurrentFree(ptr1);
        cout << "  释放成功" << endl;
    }
    
    // 最大大小
    cout << "测试最大大小 (256KB):" << endl;
    void* ptr2 = ConcurrentAlloc(256 * 1024);
    if(ptr2)
    {
        cout << "  申请成功,地址: " << ptr2 << endl;
        memset(ptr2, 0xFF, 256 * 1024);
        ConcurrentFree(ptr2);
        cout << "  释放成功" << endl;
    }
    
    // 对齐边界
    cout << "测试对齐边界:" << endl;
    std::vector<size_t> boundarySizes = {128, 129, 1024, 1025, 8*1024, 8*1024+1, 64*1024, 64*1024+1};
    for(auto size : boundarySizes)
    {
        void* ptr = ConcurrentAlloc(size);
        if(ptr)
        {
            cout << "  " << size << " 字节: 申请并释放成功" << endl;
            ConcurrentFree(ptr);
        }
    }
    
    cout << endl;
}

int main()
{
    cout << "========================================" << endl;
    cout << "    内存池归还功能测试程序" << endl;
    cout << "========================================" << endl;
    cout << endl;
    
    // 1. 基本申请释放测试
    TestBasicAllocFree();
    
    // 2. 批量申请释放测试
    TestBatchAllocFree();
    
    // 3. 交替申请释放测试
    TestInterleavedAllocFree();
    
    // 4. 内存重用测试
    TestMemoryReuse();
    
    // 5. 混合大小申请释放测试
    TestMixedSizeAllocFree();
    
    // 6. 多线程并发测试
    TestMultiThreadAllocFree();
    
    // 7. 压力测试
    TestStressAllocFree();
    
    // 8. 边界情况测试
    TestEdgeCases();
    
    cout << "========================================" << endl;
    cout << "    所有测试完成!" << endl;
    cout << "========================================" << endl;
    
    return 0;
}
