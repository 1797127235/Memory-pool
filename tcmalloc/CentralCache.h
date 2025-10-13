#pragma once
#include "Common.h"
#include"PageCache.h"

class CentralCache {
public:
    static CentralCache* GetInstance() {
        return &_sInst;
    }
    //获取一个非空span
    Span* GetOneSpan(SpanList& list,size_t byte_size);

    //中心缓存获取一定数量的对象
    size_t FetchRangeObj(void*& start,void*& end,size_t batchNum,size_t size);

    //将一定数量的对象释放给span跨度
    void ReleaseList(void* start, size_t byte_size);
private:
    SpanList _spanList[NFREELIST];
private:
    CentralCache(const CentralCache&) = delete;
    CentralCache(){}

    static CentralCache _sInst;
};


