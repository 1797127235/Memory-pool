#pragma once
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <new>
#include <vector>
#include <algorithm>
#include <utility>
#include <memory> 

#include"Common.hpp"
#ifdef _WIN32
  #define NOMINMAX
  #include <windows.h>
#else
  #include <sys/mman.h>
  #include <unistd.h>
  #ifndef MAP_ANONYMOUS
    #define MAP_ANONYMOUS MAP_ANON
  #endif
#endif

//-------------------------------
// ObjectPool专用内存管理接口（按字节）
//-------------------------------
namespace ObjectPoolMemory {
    // 获取系统页大小
    inline std::size_t page_size() {
#ifdef _WIN32
        SYSTEM_INFO info;
        GetSystemInfo(&info);
        return static_cast<std::size_t>(info.dwPageSize);
#else
        long sz = ::sysconf(_SC_PAGE_SIZE);
        return (sz > 0) ? static_cast<std::size_t>(sz) : 4096u;
#endif
    }

    // 向上对齐到指定边界
    inline std::size_t round_up(std::size_t bytes, std::size_t align) {
        assert(align && ((align & (align - 1)) == 0));
        const std::size_t add = align - 1;
        if (bytes > (std::size_t)-1 - add) {
            throw std::bad_alloc();
        }
        return (bytes + add) & ~add;
    }

    // 按字节数申请内存
    inline void* alloc_bytes(std::size_t bytes) {
        if (bytes == 0) bytes = page_size();
#ifdef _WIN32
        void* p = ::VirtualAlloc(nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!p) throw std::bad_alloc();
        return p;
#else
        void* p = ::mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (p == MAP_FAILED) throw std::bad_alloc();
        return p;
#endif
    }

    // 按字节数释放内存
    inline void free_bytes(void* p, std::size_t bytes) {
        if (!p) return;
#ifdef _WIN32
        (void)bytes;
        ::VirtualFree(p, 0, MEM_RELEASE);
#else
        ::munmap(p, bytes);
#endif
    }
}

//===============================
// ObjectPool<T>
//  - 非线程安全（需要可加外部锁）
//  - 批量页对齐申请
//  - std::align 块内对齐到 alignof(T)
//  - free list 复用
//===============================
template <class T>
class ObjectPool {
public:
    ObjectPool() = default;

    ~ObjectPool() {
        // 释放所有大块
        for (auto& c : _chunks) {
            ObjectPoolMemory::free_bytes(c.ptr, c.bytes);
        }
    }

    ObjectPool(const ObjectPool&) = delete;
    ObjectPool& operator=(const ObjectPool&) = delete;

    // 构造对象
    template <typename... Args>
    T* Construct(Args&&... args) {
        void* mem = NewRaw();
        return new (mem) T(std::forward<Args>(args)...);
    }

    // 析构并回收
    void Destroy(T* obj) {
        if (!obj) return;
        obj->~T();
        DeleteRaw(obj);
    }

private:
    struct Chunk
    {
        void* ptr;
        std::size_t bytes;
    };

    // 从池中取出原始内存（未构造）
    void* NewRaw() {
        // 先走 free-list
        if (_freeList) {
            void* node = _freeList;
            _freeList = *reinterpret_cast<void**>(_freeList);
            return node;
        }

        // 按 T 的对齐与大小切
        const std::size_t align  = std::max<std::size_t>(alignof(T), alignof(void*));
        //对齐后的对齐大小
        std::size_t stride = ObjectPoolMemory::round_up(sizeof(T), align);
        //确保能放 next 指针
        if (stride < sizeof(void*)) stride = ObjectPoolMemory::round_up(sizeof(void*), align); 
        
        //剩余的空间不足
        if (_remainBytes < stride) {
            grow_chunk(stride, align);
            // grow 后优先把整块预切到 free-list，更快
            // 预切会把 _memory/_remainBytes 置空；这里直接从 free-list 取
            if (!_freeList) throw std::bad_alloc(); // 理论上不会
            void* node = _freeList;
            _freeList = *reinterpret_cast<void**>(_freeList);
            return node;
        }

        // bump-pointer（仅当你选择不预切）
        void* p = _memory;
        _memory += stride;
        _remainBytes -= stride;
        return p;
    }

    // 回收到 free-list
    void DeleteRaw(void* p) {
        *reinterpret_cast<void**>(p) = _freeList;
        _freeList = p;
    }

    // 申请大块并对齐，预切到 free-list
    void grow_chunk(std::size_t stride, std::size_t align) {
        //系统页的大小
        const std::size_t ps = ObjectPoolMemory::page_size();

        // 目标块大小：至少 128 KiB；尽量容纳 64 个块
        // want = max(128KiB, stride * 64)，并向上取整到页大小
        std::size_t want = 128u * 1024u;

        // 溢出安全：stride * 64
        const std::size_t factor = 64u;
        std::size_t want_alt;

        //当前对象大小很大
        if (stride != 0 && (std::size_t)-1 / stride < factor) {
            want_alt = (std::size_t)-1; // 极端情况下让下面的 round_up 抛 bad_alloc
        } else {
            want_alt = stride * factor;
        }
        if (want_alt > want) want = want_alt;

        // 至少留出对齐回退余量：std::align 需要 space >= size + (align-1)
        // 我们让 want += (align - 1) 的上界空间（可选但更稳）
        if ((std::size_t)-1 - want < (align - 1)) throw std::bad_alloc();

        //加上最坏情况需要的字节
        want += (align - 1);

        //向上取整到页大小
        want = ObjectPoolMemory::round_up(want, ps);

        // 使用ObjectPoolMemory中的alloc_bytes，按字节数分配
        void* blk = ObjectPoolMemory::alloc_bytes(want);
        _chunks.push_back(Chunk{blk, want});

        // 在块内对齐到 align
        void* p = blk;
        //新申请的内存大小
        std::size_t space = want;
        //对齐
        if (!std::align(align, stride, p, space)) {
            throw std::bad_alloc();
        }

        // 现在 [p, p+space) 可用；将其按 stride 预切到 free-list
        char* base = reinterpret_cast<char*>(p);
        const std::size_t blocks = space / stride;

        for (std::size_t i = 0; i < blocks; ++i) {
            char* node = base + i * stride;
            *reinterpret_cast<void**>(node) = _freeList;
            _freeList = node;
        }

        // 预切后，不保留 bump-pointer 空间（全部进 free-list）
        _memory = nullptr;
        _remainBytes = 0;
    }

private:
    // bump-pointer（仅在没有预切或你改成混合策略时使用）
    char*        _memory      = nullptr;
    std::size_t  _remainBytes = 0;

    // 单链表空闲链
    void*   _freeList    = nullptr;

    // 所有大块，析构时统一释放
    std::vector<Chunk> _chunks;
};
