#pragma  once

#include<mutex>
#include<unordered_map>
#include"Common.h"
#include"ObjectPool.h"
#include"PageMap.h"


class PageCache {
public:
    static PageCache* GetInstance() {
        return &_sInst;
    }

    std::mutex& Getmtx() {
        return _mtx;
    }

    Span* NewSpan(size_t k);

    //获取从对象到span的映射
    Span* MapObjToSpan(void* obj);

	// 释放空闲span回到Pagecache，并合并相邻的span
	void ReleaseSpanToPageCache(Span* span);

private:
    PageCache() {}
    PageCache(const PageCache &) = delete;
    PageCache &operator=(const PageCache &) = delete;
    ~PageCache() {}

    ObjectPool<Span> _spanPool;

    std::mutex _mtx;

    static PageCache _sInst;

    SpanList _spanLists[NPAGES];
    std::unordered_map<PAGE_ID,Span*> _idSpanMap;

    TCMalloc_PageMap3<64 - PAGE_SHIFT> _pageMap;
};