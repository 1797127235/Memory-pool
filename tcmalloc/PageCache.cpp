#include "PageCache.h"

PageCache PageCache::_sInst;

//获取一个k页的span
Span *PageCache::NewSpan(size_t k) {
    assert(k > 0);

    if(!_spanLists[k].Empty()) {
        Span* kspan = _spanLists[k].PopFront();
        
        // 建立页号到span的映射
        for(size_t i = 0; i < kspan->_n; ++i)
        {
            _idSpanMap[kspan->_pageId + i] = kspan;
        }

        return kspan;
    }

    //检查后续是否有剩余的大块span

    for(int i = k + 1; i < NPAGES; ++i)
    {
        if(!_spanLists[i].Empty())
        {
            //对span进行切分
            Span* nspan = _spanLists[i].PopFront();

            Span* kspan = new Span();
            kspan->_pageId = nspan->_pageId;
            kspan->_n = k;

            nspan->_pageId += k;
            nspan->_n -= k;
            _spanLists[nspan->_n].PushFront(nspan);
            
            // 建立nspan的映射
            _idSpanMap[nspan->_pageId] = nspan;
            _idSpanMap[nspan->_pageId + nspan->_n - 1] = nspan;
            
            // 建立kspan的映射
            for(size_t i = 0; i < kspan->_n; ++i)
            {
                _idSpanMap[kspan->_pageId + i] = kspan;
            }

            return kspan;
        }
    }


    Span* bigspan = new Span();
    void* ptr = SystemAlloc(NPAGES - 1);
    bigspan->_pageId = (PAGE_ID)((uintptr_t)ptr >> PAGE_SHIFT);
    bigspan->_n = NPAGES - 1;

    _spanLists[NPAGES - 1].PushFront(bigspan);
    
    // 建立映射
    _idSpanMap[bigspan->_pageId] = bigspan;
    _idSpanMap[bigspan->_pageId + bigspan->_n - 1] = bigspan;

    return NewSpan(k);
}

Span* PageCache::MapObjToSpan(void* obj)
{
    PAGE_ID pageId = (PAGE_ID)((uintptr_t)obj >> PAGE_SHIFT);
    
    auto ret = _idSpanMap.find(pageId);

    assert(ret != _idSpanMap.end());
    
    return ret->second;
}


void PageCache::ReleaseSpanToPageCache(Span* span)
{
    //返回的span合并成更大的页


    while(1)
    {
        PAGE_ID prevId = span->_pageId - 1;
        
        auto it = _idSpanMap.find(prevId);
        if(it == _idSpanMap.end())
        {
            break;
        }

        Span* prev = it->second;
        if(prev->_isUse)
        {
            break;
        }
        if(prev->_n + span->_n > NPAGES - 1)
        {
            break;
        }

        span->_pageId = prev->_pageId;
        
        span->_n += prev->_n;

        _spanLists[prev->_n]._mtx.lock();
        _spanLists[prev->_n].Erase(prev);
        _spanLists[prev->_n]._mtx.unlock();
    }


    while (1)
	{
		PAGE_ID nextId = span->_pageId + span->_n;

		auto it = _idSpanMap.find(nextId);
		if (it == _idSpanMap.end())
		{
			break;
		}

		Span* nextSpan = it->second;
		if (nextSpan->_isUse == true)
		{
			break;
		}

		if (nextSpan->_n + span->_n > NPAGES-1)
		{
			break;
		}

		span->_n += nextSpan->_n;

		_spanLists[nextSpan->_n]._mtx.lock();
		_spanLists[nextSpan->_n].Erase(nextSpan);
		_spanLists[nextSpan->_n]._mtx.unlock();
	}

    _spanLists[span->_n].PushFront(span);
    span->_isUse = false;

    _idSpanMap[span->_pageId] = span;
    _idSpanMap[span->_pageId + span->_n - 1] = span;

}

