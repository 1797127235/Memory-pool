#include "PageCache.h"

PageCache PageCache::_sInst;

//获取一个k页的span
Span *PageCache::NewSpan(size_t k) {
    assert(k > 0);

    if(!_spanLists[k].Empty()) {
        Span* kspan = _spanLists[k].PopFront();

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

            return kspan;
        }
    }


    Span* bigspan = new Span();
    void* ptr = SystemAlloc(NPAGES - 1);
    bigspan->_pageId = (PAGE_ID)((uintptr_t)ptr >> PAGE_SHIFT);
    bigspan->_n = NPAGES - 1;

    _spanLists[NPAGES - 1].PushFront(bigspan);

    return NewSpan(k);
}

Span* PageCache::MapObjToSpan(void* obj)
{
    PAGE_ID pageId = (PAGE_ID)(obj >> PAGE_SHIFT);
    
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
        
        auto ret = (Span*)_idSpanMap.get(prevId);
        if(ret == nullptr)
        {
            break;
        }

        if(ret->_isUse)
        {
            break;
        }

        Span* prev = ret;
        if(prev->_n + span->_n > NPAGES - 1)
        {
            break;
        }

        span->_pageId = prev->_pageId;
        
        span->_n += prev->_n;

        _spanList[prev->_n]._mtx.lock();
        _spanList[prev->_n].Erase(prev);
        _spanList[prev->_n]._mtx.unlock();
    }


    while (1)
	{
		PAGE_ID nextId = span->_pageId + span->_n;

		auto ret = (Span*)_idSpanMap.get(nextId);
		if (ret == nullptr)
		{
			break;
		}

		Span* nextSpan = ret;
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

    _spanList[span->_n].PushFront(span);
    span->_isUse = false;

    _idSpanMap.set(span->_pageId,span);
    _idSpanMap.set(span->_pageId + span->_n - 1,span);

}

