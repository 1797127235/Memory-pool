#include "PageCache.h"

PageCache PageCache::_sInst;

//获取一个k页的span
Span *PageCache::NewSpan(size_t k) {
    assert(k > 0);

    if(k > NPAGES - 1)
    {
        void* ptr = SystemAlloc(k);

        Span*span = _spanPool.New();
        span->_pageId = (PAGE_ID)((uintptr_t)ptr >> PAGE_SHIFT);
        span->_n = k;
        span->_isUse = true;

        // 在用大页：逐页映射，保证 MapObjToSpan 命中
        _pageMap.Ensure(span->_pageId, span->_n);
        for (size_t i = 0; i < span->_n; ++i)
        {
            _pageMap.set(span->_pageId + i, span);
        }

        return span;
    }

    if(!_spanLists[k].Empty()) {
        Span* kspan = _spanLists[k].PopFront();
        
        // 建立页号到span的映射（在用：逐页映射）
        _pageMap.Ensure(kspan->_pageId, kspan->_n);
        for(size_t i = 0; i < kspan->_n; ++i)
        {
            _pageMap.set(kspan->_pageId + i, kspan);
        }

        return kspan;
    }

    //检查后续是否有剩余的大块span

    for(size_t i = k + 1; i < NPAGES; ++i)
    {
        if(!_spanLists[i].Empty())
        {
            //对span进行切分
            Span* nspan = _spanLists[i].PopFront();

            Span* kspan = _spanPool.New();
            kspan->_pageId = nspan->_pageId;
            kspan->_n = k;

            nspan->_pageId += k;
            nspan->_n -= k;
            _spanLists[nspan->_n].PushFront(nspan);
            
            // 建立nspan的映射（空闲：仅首尾）
            _pageMap.Ensure(nspan->_pageId, nspan->_n);
            _pageMap.set(nspan->_pageId, nspan);
            _pageMap.set(nspan->_pageId + nspan->_n - 1, nspan);
            
            // 建立kspan的映射（在用：逐页）
            _pageMap.Ensure(kspan->_pageId, kspan->_n);
            for(size_t i = 0; i < kspan->_n; ++i)
            {
                _pageMap.set(kspan->_pageId + i, kspan);
            }

            return kspan;
        }
    }


    Span* bigspan = _spanPool.New();
    void* ptr = SystemAlloc(NPAGES - 1);
    bigspan->_pageId = (PAGE_ID)((uintptr_t)ptr >> PAGE_SHIFT);
    bigspan->_n = NPAGES - 1;

    _spanLists[NPAGES - 1].PushFront(bigspan);
    
    // 建立映射（空闲：仅首尾）
    _pageMap.Ensure(bigspan->_pageId, bigspan->_n);
    _pageMap.set(bigspan->_pageId, bigspan);
    _pageMap.set(bigspan->_pageId + bigspan->_n - 1, bigspan);

    return NewSpan(k);
}

Span* PageCache::MapObjToSpan(void* obj)
{
    PAGE_ID pageId = (PAGE_ID)((uintptr_t)obj >> PAGE_SHIFT);
    
    void* v = _pageMap.get(pageId);
    assert(v != nullptr);
    return (Span*)v;
}


void PageCache::ReleaseSpanToPageCache(Span* span)
{

    if(span->_n > NPAGES - 1)
    {
        void* ptr = (void*)(span->_pageId << PAGE_SHIFT);
        // 清理映射（逐页清空）
        _pageMap.Ensure(span->_pageId, span->_n);
        for (PAGE_ID pid = span->_pageId; pid < span->_pageId + span->_n; ++pid)
        {
            _pageMap.set(pid, nullptr);
        }
        SystemFree(ptr, span->_n);
        _spanPool.Delete(span);
        return;
    }
    //返回的span合并成更大的页
    while(1)
    {
        PAGE_ID prevId = span->_pageId - 1;
        
        Span* prev = (Span*)_pageMap.get(prevId);  //前一个页所在span（其末页）
        if(prev == nullptr)
        {
            break;
        }

        if(prev->_isUse)
        {
            break;
        }
        if(prev->_n + span->_n > NPAGES - 1)
        {
            break;
        }

        // 清理前驱span的边界映射
        _pageMap.Ensure(prev->_pageId, prev->_n);
        _pageMap.set(prev->_pageId, nullptr);
        _pageMap.set(prev->_pageId + prev->_n - 1, nullptr);

        span->_pageId = prev->_pageId;
        
        span->_n += prev->_n;

        _spanLists[prev->_n].Erase(prev);
        _spanPool.Delete(prev);
    }

    while (1)
	{
		PAGE_ID nextId = span->_pageId + span->_n;

		        Span* nextSpan = (Span*)_pageMap.get(nextId);
        if (nextSpan == nullptr)
        {
            break;
        }
		if (nextSpan->_isUse == true)
		{
			break;
		}

		if (nextSpan->_n + span->_n > NPAGES-1)
		{
			break;
		}

		        // 清理后继span的边界映射
        _pageMap.Ensure(nextSpan->_pageId, nextSpan->_n);
        _pageMap.set(nextSpan->_pageId, nullptr);
        _pageMap.set(nextSpan->_pageId + nextSpan->_n - 1, nullptr);

		span->_n += nextSpan->_n;

		_spanLists[nextSpan->_n].Erase(nextSpan);
        _spanPool.Delete(nextSpan);
	}

    // 空闲：仅首尾映射，清理内部页映射
    _pageMap.Ensure(span->_pageId, span->_n);
    if (span->_n >= 3) {
        for (PAGE_ID pid = span->_pageId + 1; pid < span->_pageId + span->_n - 1; ++pid) {
            _pageMap.set(pid, nullptr);
        }
    }
    _pageMap.set(span->_pageId, span);
    _pageMap.set(span->_pageId + span->_n - 1, span);

    _spanLists[span->_n].PushFront(span);
    span->_isUse = false;

}

