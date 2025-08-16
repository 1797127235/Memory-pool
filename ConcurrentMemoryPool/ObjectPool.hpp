#pragma once
#include <iostream>
#include <vector>
#include <cstdlib>
#include <memory>
#include <cassert>
using std::cout;
using std::endl;

#ifdef _WIN32
#define MOMINMAX
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif
#endif

// 获取系统页大小
inline size_t page_size()
{
#ifdef _WIN32
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    return info.dwPageSize;
#else
    long sz = ::sysconf(_SC_PAGE_SIZE);
    return (sz > 0) ? static_cast<size_t>(sz) : 4096;
#endif
}


// 向 OS 申请整页内存（返回块指针与字节数）
inline void *SystemAlloc(size_t bytes)
{
#ifdef _WIN32
    void *p = ::VirtualAlloc(nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!p)
        throw std::bad_alloc();
    return p;
#else
    void *p = ::mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED)
        throw std::bad_alloc();
    return p;
#endif
}

// 释放内存
inline void SystemFree(void *p, size_t bytes = 0)
{
#ifdef _WIN32
    ::VirtualFree(p, 0, MEM_RELEASE);
#else
    ::munmap(p, bytes);
#endif
}

// 向上取整到align的倍数
inline size_t round_up(size_t bytes, size_t align)
{
    assert(align&&((align&(align-1))==0));
    return ((bytes + align - 1) & ~(align - 1));
}

template <class T>
class ObjectPool
{
public:
    ObjectPool() :
     _memory(nullptr),
    _remainBytes(0),
    _freeList(nullptr)
    {
        
    }

    ~ObjectPool()
    {
        for (auto &c : _chunks)
        {
            SystemFree(c.ptr, c.bytes);
        }
    }

    template <typename... Args>
    T *Construct(Args &&...args)
    {
        void *mem = New();
        return new (mem) T(std::forward<Args>(args)...);
    }

    void Destroy(T *obj)
    {
        if (obj)
            obj->~T();
        Delete(obj);
    }

    ObjectPool(const ObjectPool &) = delete;
    ObjectPool &operator=(const ObjectPool &) = delete;

private:
    struct Chunk
    {
        void *ptr;
        size_t bytes;
    };
    T *New()
    {
        // 先从空闲链表复用
        if (_freeList)
        {
            void *node = _freeList;
            _freeList = *reinterpret_cast<void **>(_freeList);
            return reinterpret_cast<T *>(node);
        }

        const size_t align = std::max(alignof(T), alignof(void *)); // 对齐数
        const size_t stride = round_up(sizeof(T), align);           // 对象大小

        if (_remainBytes < stride)
        {
            grow_chunk(stride, align);
        }

        T *obj = reinterpret_cast<T *>(_memory);
        _memory += stride;
        _remainBytes -= stride;
        return obj;
    }

    void Delete(T *obj)
    {
        // obj->~T();
        //  头插：把 obj 的起始处当作 "next" 指针存放
        *reinterpret_cast<void **>(obj) = _freeList;
        _freeList = obj;
    }

    // 申请大块内存并将_memory对齐到align

    void grow_chunk(size_t stride, size_t align)
    {
        // 获取系统页大小
        const size_t ps = page_size();

        // 目标块大小 至少128KB
        size_t want = 128 * 1024;
        // 小对象尽量多装几份
        if (want < stride * 64)
            want = stride * 64;

        // 向上取整到页大小
        want = round_up(want, ps);

        // 申请大块内存
        void *blk = SystemAlloc(want);
        _chunks.push_back(Chunk{blk, want});

        _memory = reinterpret_cast<char*>(blk);
        _remainBytes = want;

        void *p = _memory;
        size_t space = _remainBytes;
        // std::align
        if (std::align(align, stride, p, space))
        {
            _memory = reinterpret_cast<char *>(p);
            _remainBytes = space;
        }
        else
        {
            throw std::bad_alloc();
        }
    }

private:
    char *_memory = nullptr;   // 当前大块内存
    size_t _remainBytes = 0;   // 当前大块内存剩余字节数
    void *_freeList = nullptr; // 空闲链表

    std::vector<Chunk> _chunks; // 所有大块指针,析构时统一释放
};