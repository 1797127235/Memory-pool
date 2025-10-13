#include "PageCache.h"

PageCache PageCache::_sInst;

//获取一个k页的span
Span *PageCache::NewSpan(size_t k) {
    assert(k > 0);

    if(!_spanLists[k].empty()) {
        Span* kspan = _spanLists[k].PopFront();

        return kspan;
    }

    //检查后续是否有剩余的大块span

    for(int i = k + 1; i < NPAGES; ++i)
    {
        if(!_spanLists[i].empty())
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
    bigspan->_pageId = (PAGE_ID)(ptr >> PAGE_SHIFT);
    bigspan->_n = NPAGES - 1;

    _spanLists[NPAGES - 1].PushFront(bigspan);

    return NewSpan(k);
}
