#pragma once
#include"Common.hpp"
#include <mutex>
#include <unistd.h>
#include<unordered_map>

#include"ObjectPool.hpp"
#include"RadixSpanMap.hpp"

class PageCache
{
private:
    PageCache(){}
    ~PageCache(){}
    PageCache(const PageCache&) = delete;
    PageCache& operator=(const PageCache&) = delete;
    PageCache(PageCache&&) = delete;
    PageCache& operator=(PageCache&&) = delete;
    static PageCache _Inst;

    ObjectPool<Span> _spanPool;
private:
    //挂着k页的span
    SpanList _spanlist[NPAGES];

    //内存到span的映射
    std::unordered_map<PAGE_ID,Span*> _idSpanMap;

    RadixSpanMap _radixSpanMap;
// 保留但不再作为分配慢路径互斥，而用于偶发全局调试
public:
    std::mutex _mtx;
public:
    //创建一个k页的span
    Span* NewSpan(size_t k)
    {
        assert(k > 0 && k < NPAGES);

        // 1) 目标桶直接命中
        
        //std::lock_guard<std::mutex> lock(_spanlist[k]._mtx);
        if (!_spanlist[k].Empty())
        {
            Span* s = _spanlist[k].Pop();
        s->useCount = 0;
        s->_objSize = 0;
        s->_freeList = nullptr;
        s->_isUse = false;
        return s;
        }
        

        // 2) 从更大桶切分
        for (size_t i = k + 1; i < NPAGES; ++i)
        {
            if (!_spanlist[i].Empty())
            {
                Span* nspan = _spanlist[i].Pop();

                // 切出 k 页
                // Span* kspan = new Span();
                Span* kspan = _spanPool.Construct();
                kspan->_page_id = nspan->_page_id;
                kspan->_n = k;
                kspan->_objSize = 0;
                kspan->_freeList = nullptr;
                kspan->_isUse = false;
                kspan->useCount = 0;

                // 剩余回收到对应桶
                nspan->_page_id += k;
                nspan->_n = i - k; 
                nspan->useCount = 0;
                assert(nspan->_n > 0 && nspan->_n < NPAGES);
                _spanlist[nspan->_n].PushFront(nspan);


                // MapSpan(nspan);
                // MapSpan(kspan);
                _radixSpanMap.MapRange(nspan->_page_id, nspan->_n, nspan);
                _radixSpanMap.MapRange(kspan->_page_id, kspan->_n, kspan);
                return kspan;
            }
        }

        // 3) 向系统申请 NPAGES-1 页，再按 k 切
        void* ptr = SystemAlloc(NPAGES - 1);

        Span* bigspan = _spanPool.Construct();
        bigspan->_page_id = (PAGE_ID)((uintptr_t)ptr >> PAGE_SHIFT);
        bigspan->_n = NPAGES - 1;
        bigspan->_objSize = 0;
        bigspan->_freeList = nullptr;
        bigspan->_isUse = false;
        bigspan->useCount = 0;

        if (k == NPAGES - 1)
        {
            _radixSpanMap.MapRange(bigspan->_page_id, bigspan->_n, bigspan);
            return bigspan;
        }

        _spanlist[NPAGES - 1].PushFront(bigspan);
        _radixSpanMap.MapRange(bigspan->_page_id, bigspan->_n, bigspan);
        return NewSpan(k);
    }

    void ReleaseSpanToPageCache(Span* span)
    {
        // 大于128 page的直接还给堆
        if (span->_n > NPAGES-1)
        {
            void* ptr = (void*)(span->_page_id << PAGE_SHIFT);
            size_t kpage = span->_n;
            SystemFree(ptr,kpage);
            // delete span;
            _spanPool.Destroy(span);

            return;
        }

        // 对span前后的页，尝试进行合并，缓解内存碎片问题
        while (1)
        {
            PAGE_ID prevId = span->_page_id - 1;
            Span* prevSpan = _radixSpanMap.Lookup(prevId);

            // 前面相邻页的span在使用，不合并了
            if (prevSpan == nullptr || prevSpan->_isUse == true)
            {
                break;
            }

            // 合并出超过128页的span没办法管理，不合并了
            if (prevSpan->_n + span->_n > NPAGES-1)
            {
                break;
            }

            span->_page_id = prevSpan->_page_id;
            span->_n += prevSpan->_n;

            _spanlist[prevSpan->_n].Erase(prevSpan);
            _spanPool.Destroy(prevSpan);
        }

        // 向后合并
        while (1)
        {
            PAGE_ID nextId = span->_page_id + span->_n;
            Span* nextSpan = _radixSpanMap.Lookup(nextId);
            if (nextSpan == nullptr || nextSpan->_isUse == true)
            {
                break;
            }


            if (nextSpan->_n + span->_n > NPAGES-1)
            {
                break;
            }

            span->_n += nextSpan->_n;

            _spanlist[nextSpan->_n].Erase(nextSpan);
            _spanPool.Destroy(nextSpan);
        }

        _spanlist[span->_n].PushFront(span);
        span->_isUse = false;
        //_radixSpanMap.MapRange(span->_page_id, span->_n, span);
        
        //只要span的头尾页，中间的页不映射
        _radixSpanMap.MapRange(span->_page_id, 1, span);
        _radixSpanMap.MapRange(span->_page_id + span->_n - 1, 1, span);
    }


    static PageCache* GetInstance()
    {
        return &_Inst;
    }

    Span* MapObjectToSpanNoLock(PAGE_ID id)
    {
        Span* span = _radixSpanMap.Lookup(id);
        if (span == nullptr)
        {
            return nullptr;
        }
        return span;
    }

    Span* MapObjectToSpan(void* obj)
    {
        PAGE_ID id = (PAGE_ID)((uintptr_t)obj >> PAGE_SHIFT);
        std::lock_guard<std::mutex> g(_mtx);
        return MapObjectToSpanNoLock(id);
    }
};

inline PageCache PageCache::_Inst{};