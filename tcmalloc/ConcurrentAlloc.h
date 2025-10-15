#pragma  once

#include"ThreadCache.h"
#include"PageCache.h"
#include"ObjectPool.h"
#include"Common.h"


static void* ConcurrentAlloc(size_t size) {

    if(size > MAX_BYTES)
    {
        size_t alignsize = SizeClass::RoundUp(size);
        size_t kpage = alignsize >> PAGE_SHIFT;

        //直接向PageCache申请

        PageCache::GetInstance()->Getmtx().lock();
        Span* span = PageCache::GetInstance()->NewSpan(kpage);
        span->_objSize = size;
        span->_isUse = true;
        PageCache::GetInstance()->Getmtx().unlock();


        void* ptr = (void*)(span->_pageId << PAGE_SHIFT);

        return ptr;
    }
    else
    {
        if(TlsThreadCache == nullptr)
        {
            static ObjectPool<ThreadCache> tcpool;
            static std::mutex m;
            std::lock_guard<std::mutex> lg(m);
            if (TlsThreadCache == nullptr)
                TlsThreadCache = tcpool.New();
        }
        
        return TlsThreadCache->Allocate(size);
    }
}


static void ConcurrentFree(void* ptr) {

    PageCache::GetInstance()->Getmtx().lock();
    Span* span = PageCache::GetInstance()->MapObjToSpan(ptr);
    PageCache::GetInstance()->Getmtx().unlock();

    if(span->_objSize > MAX_BYTES)
    {
        PageCache::GetInstance()->Getmtx().lock();
        PageCache::GetInstance()->ReleaseSpanToPageCache(span);
        PageCache::GetInstance()->Getmtx().unlock();
    }
    else
    {
        TlsThreadCache->Deallocate(ptr,span->_objSize);
    }
}