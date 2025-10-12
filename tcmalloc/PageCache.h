#pragma  once

#include<mutex>
#include<unordered_map>
#include"Common.h"


class PageCache {
public:
    static PageCache* GetInstance() {
        return &_sInst;
    }

    std::mutex& Getmtx() {
        return _mtx;
    }

    Span* NewSpan(size_t k);

private:
    PageCache() {}
    PageCache(const PageCache &) = delete;
    PageCache &operator=(const PageCache &) = delete;
    ~PageCache() {}

    std::mutex _mtx;

    static PageCache _sInst;

    SpanList _spanLists[NPAGES];
    std::unordered_map<PAGE_ID,Span*> _idSpanMap;

};