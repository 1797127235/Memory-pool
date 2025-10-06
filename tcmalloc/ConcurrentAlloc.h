#pragma  once

#include"ThreadCache.h"


static void* ConcurrentAlloc(size_t size) {
    if (TlsThreadCache == nullptr) {
        TlsThreadCache = new ThreadCache();
    }

    return TlsThreadCache->Allocate(size);
}


static void ConcurrentFree(void* ptr) {
    assert(TlsThreadCache);
    //TlsThreadCache->Deallocate(ptr);
}