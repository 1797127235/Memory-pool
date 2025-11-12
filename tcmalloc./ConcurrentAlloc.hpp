#pragma once
#include"ThreadCache.hpp"
#include <cstddef>


inline void* ConcurrentAlloc(size_t size)
{
    if(size == 0) return nullptr;
    if (size > MAX_BYTES)
    {
        // 计算一页的大小
        const size_t page_size = size_t(1) << PAGE_SHIFT;

        //超过256kb的内存直接交给PageCache
        size_t kpage = (size + page_size - 1) / page_size;

        PageCache::GetInstance()->_mtx.lock();
        Span* span = PageCache::GetInstance()->NewSpan(kpage);
        PageCache::GetInstance()->_mtx.unlock();
        span->_isUse = true;
        return reinterpret_cast<void*>(span->_page_id << PAGE_SHIFT);
    }



    if(g_threadCache == nullptr)
    {
        g_threadCache = new ThreadCache();
    }
    return g_threadCache->Allocate(size);
}


inline void ConcurrentFree(void* p)
{
    if (!p) return;

    PAGE_ID id = (PAGE_ID)((uintptr_t)p >> PAGE_SHIFT);
    Span* span = nullptr;
    size_t objSize = 0;
    bool isLargeObj = false;

    //查看当前内存块是否在大块内存里面,超过256kb的内存直接交给PageCache
    {
        std::lock_guard<std::mutex> g(PageCache::GetInstance()->_mtx);
        span = PageCache::GetInstance()->MapObjectToSpanNoLock(id);
        if (!span) {
            assert(false && "Pointer not from this allocator");
            return;
        }
        objSize = span->_objSize;
        isLargeObj = (objSize == 0 || objSize > MAX_BYTES);
        
        if (isLargeObj) {
            span->_isUse = false;
            PageCache::GetInstance()->ReleaseSpanToPageCache(span);
            return;
        }
    }

    if (g_threadCache == nullptr) {
        NextObj(p) = nullptr;
        CentralCache::GetInstance()->ReleaseListToSpans(p, objSize);
        return;
    }
    g_threadCache->Deallocate(p, objSize);
}