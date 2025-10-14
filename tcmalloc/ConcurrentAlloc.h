#pragma  once

#include"ThreadCache.h"
#include"PageCache.h"


static void* ConcurrentAlloc(size_t size) {
    if (TlsThreadCache == nullptr) {
        TlsThreadCache = new ThreadCache();
    }

    return TlsThreadCache->Allocate(size);
}


static void ConcurrentFree(void* ptr) {
    assert(TlsThreadCache);
    Span* span = PageCache::GetInstance()->MapObjToSpan(ptr);

    size_t objsize = span->_objSize;
    TlsThreadCache->Deallocate(ptr,objsize);
}