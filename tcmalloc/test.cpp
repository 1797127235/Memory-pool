#include"ConcurrentAlloc.h"
#include<vector>
#include<thread>

// 测试单次内存申请
void TestSingleAlloc()
{
    cout << "=== 测试单次内存申请 ===" << endl;
    
    // 测试不同大小的内存申请
    std::vector<size_t> sizes = {8, 16, 32, 64, 128, 256, 512, 1024, 
                                  2*1024, 4*1024, 8*1024, 16*1024, 
                                  32*1024, 64*1024, 128*1024, 256*1024};
    
    for(auto size : sizes)
    {
        void* ptr = ConcurrentAlloc(size);
        if(ptr != nullptr)
        {
            cout << "成功申请 " << size << " 字节内存,地址: " << ptr << endl;
            // 写入数据验证内存可用
            memset(ptr, 0xAB, size);
        }
        else
        {
            cout << "申请 " << size << " 字节内存失败!" << endl;
        }
    }
    cout << endl;
}

// 测试批量内存申请
void TestBatchAlloc()
{
    cout << "=== 测试批量内存申请 ===" << endl;
    
    const int count = 100;
    std::vector<void*> ptrs;
    
    // 申请100个128字节的内存块
    cout << "申请 " << count << " 个 128 字节的内存块..." << endl;
    for(int i = 0; i < count; ++i)
    {
        void* ptr = ConcurrentAlloc(128);
        if(ptr != nullptr)
        {
            ptrs.push_back(ptr);
            // 写入数据
            memset(ptr, i % 256, 128);
        }
    }
    cout << "成功申请 " << ptrs.size() << " 个内存块" << endl;
    
    // 验证数据
    bool dataValid = true;
    for(size_t i = 0; i < ptrs.size(); ++i)
    {
        unsigned char* p = (unsigned char*)ptrs[i];
        unsigned char expected = i % 256;
        if(p[0] != expected)
        {
            dataValid = false;
            cout << "数据验证失败: 索引 " << i << endl;
            break;
        }
    }
    if(dataValid)
    {
        cout << "数据验证成功!" << endl;
    }
    cout << endl;
}

// 测试不同对齐区间的内存申请
void TestAlignmentRanges()
{
    cout << "=== 测试不同对齐区间 ===" << endl;
    
    // [1,128] 8byte对齐
    cout << "测试 [1,128] 区间 (8byte对齐):" << endl;
    for(size_t size : {1, 7, 8, 64, 127, 128})
    {
        void* ptr = ConcurrentAlloc(size);
        cout << "  申请 " << size << " 字节 -> 地址: " << ptr << endl;
    }
    
    // [129,1024] 16byte对齐
    cout << "测试 [129,1024] 区间 (16byte对齐):" << endl;
    for(size_t size : {129, 256, 512, 1024})
    {
        void* ptr = ConcurrentAlloc(size);
        cout << "  申请 " << size << " 字节 -> 地址: " << ptr << endl;
    }
    
    // [1025,8*1024] 128byte对齐
    cout << "测试 [1025,8K] 区间 (128byte对齐):" << endl;
    for(size_t size : {1025, 2*1024, 4*1024, 8*1024})
    {
        void* ptr = ConcurrentAlloc(size);
        cout << "  申请 " << size << " 字节 -> 地址: " << ptr << endl;
    }
    
    // [8K+1,64K] 1024byte对齐
    cout << "测试 [8K+1,64K] 区间 (1024byte对齐):" << endl;
    for(size_t size : {8*1024+1, 16*1024, 32*1024, 64*1024})
    {
        void* ptr = ConcurrentAlloc(size);
        cout << "  申请 " << size << " 字节 -> 地址: " << ptr << endl;
    }
    
    // [64K+1,256K] 8*1024byte对齐
    cout << "测试 [64K+1,256K] 区间 (8K对齐):" << endl;
    for(size_t size : {64*1024+1, 128*1024, 256*1024})
    {
        void* ptr = ConcurrentAlloc(size);
        cout << "  申请 " << size << " 字节 -> 地址: " << ptr << endl;
    }
    cout << endl;
}

// 多线程测试函数
void ThreadAllocTest(int threadId, int allocCount)
{
    cout << "线程 " << threadId << " 开始申请内存..." << endl;
    
    std::vector<void*> ptrs;
    for(int i = 0; i < allocCount; ++i)
    {
        // 申请不同大小的内存
        size_t size = (i % 10 + 1) * 128;
        void* ptr = ConcurrentAlloc(size);
        if(ptr != nullptr)
        {
            ptrs.push_back(ptr);
            // 写入线程ID标识
            memset(ptr, threadId, size);
        }
    }
    
    cout << "线程 " << threadId << " 完成,共申请 " << ptrs.size() << " 个内存块" << endl;
}

// 测试多线程并发申请
void TestMultiThreadAlloc()
{
    cout << "=== 测试多线程并发申请 ===" << endl;
    
    const int threadCount = 4;
    const int allocPerThread = 50;
    
    std::vector<std::thread> threads;
    
    for(int i = 0; i < threadCount; ++i)
    {
        threads.emplace_back(ThreadAllocTest, i, allocPerThread);
    }
    
    for(auto& t : threads)
    {
        t.join();
    }
    
    cout << "所有线程完成" << endl;
    cout << endl;
}

// 压力测试
void TestStressAlloc()
{
    cout << "=== 压力测试 ===" << endl;
    
    const int totalAllocs = 1000;
    std::vector<void*> ptrs;
    
    cout << "开始申请 " << totalAllocs << " 个随机大小的内存块..." << endl;
    
    for(int i = 0; i < totalAllocs; ++i)
    {
        // 随机大小: 1 到 256KB
        size_t size = (i * 137 % 256) * 1024 + 1;
        if(size > 256 * 1024) size = 256 * 1024;
        
        void* ptr = ConcurrentAlloc(size);
        if(ptr != nullptr)
        {
            ptrs.push_back(ptr);
            // 写入部分数据
            if(size >= 4)
            {
                *(int*)ptr = i;
            }
        }
        
        if((i + 1) % 100 == 0)
        {
            cout << "已申请 " << (i + 1) << " 个内存块..." << endl;
        }
    }
    
    cout << "压力测试完成,成功申请 " << ptrs.size() << " 个内存块" << endl;
    cout << endl;
}

int main()
{
    cout << "========================================" << endl;
    cout << "    内存池申请功能测试程序" << endl;
    cout << "========================================" << endl;
    cout << endl;
    
    // 1. 单次申请测试
    TestSingleAlloc();
    
    // 2. 批量申请测试
    TestBatchAlloc();
    
    // 3. 不同对齐区间测试
    TestAlignmentRanges();
    
    // 4. 多线程并发测试
    TestMultiThreadAlloc();
    
    // 5. 压力测试
    TestStressAlloc();
    
    cout << "========================================" << endl;
    cout << "    所有测试完成!" << endl;
    cout << "========================================" << endl;
    
    return 0;
}