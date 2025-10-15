#pragma once

#include"Common.h"

class ThreadCache
{
public:
    void* Allocate(size_t size);
    void Deallocate(void* p,size_t size);
    //从中心缓存获取对象
    void* FetchFromCentralCache(size_t index,size_t size);

    //释放对象时,链表过长时，回收内存回到中心缓存
	void ListTooLong(FreeList& list, size_t size);

private:
    FreeList _freeList[NFREELIST];
};

//线程本地存储声明（在 ThreadCache.cpp 中定义）
extern thread_local ThreadCache* TlsThreadCache;