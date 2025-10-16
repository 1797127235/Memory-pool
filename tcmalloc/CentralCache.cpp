#include "CentralCache.h"


CentralCache CentralCache::_sInst;

Span *CentralCache::GetOneSpan(SpanList &list, size_t size) {
    Span* it = list.Begin();
    while(it != list.End()) {
        if(it->_freeList != nullptr)
        {
            return it;
        }
        it = it->_next;
    }

    //此时没有空闲的向下层要
    list._mtx.unlock(); //先把上一层的锁解开

    PageCache::GetInstance()->Getmtx().lock();//给pagecache加锁

    Span* span = PageCache::GetInstance()->NewSpan(SizeClass::NumMovePage(size)); //取得一块k页的span
    
    span->_isUse = true;
    span->_objSize = size;

    PageCache::GetInstance()->Getmtx().unlock();

    //对span进行切分
    char* start = (char*)(span->_pageId << PAGE_SHIFT);
    size_t bytes = span->_n << PAGE_SHIFT;
    char* end = start + bytes;
    
    span->_freeList = start;
    start += size;
    void* tail = span->_freeList;

    int i = 1;
    while(start < end)
    {
        ++i;
        NextObj(tail) = start;
        tail = start;
        start += size;
    }
    NextObj(tail) = nullptr;

    list._mtx.lock(); //给中心缓存重新加锁
    list.PushFront(span);
    return span;
}



size_t CentralCache::FetchRangeObj(void *&start, void *&end, size_t batchNum, size_t size) {
    //先找到对应的桶
    size_t index = SizeClass::Index(size);
    _spanList[index]._mtx.lock();


    Span* span = GetOneSpan(_spanList[index],size);
    assert(span);
    assert(span->_freeList);

    start = span->_freeList;
    end = start;
    size_t i = 0;
    size_t actualNum = 1;
    //取出batchNum个内存块
    while (i < batchNum -1 && NextObj(end)) {
        end = NextObj(end);
        i++;
        actualNum++;
    }

    span->_freeList = NextObj(end);
    NextObj(end) = nullptr;
    //更新span使用计数
    span->_useCount += actualNum;
    span->_objSize = size;
    _spanList[index]._mtx.unlock();

    return actualNum;

}

//归还内存到span（按Span分组，减少锁切换）
void CentralCache::ReleaseListToSpans(void* start, size_t size) 
{
    const int index = SizeClass::Index(size);

    // 1) 在PageCache锁下，将对象链按Span分组，形成每组的start/end与计数
    std::unordered_map<Span*, void*> groupStart;
    std::unordered_map<Span*, void*> groupEnd;
    std::unordered_map<Span*, size_t> groupCount;

    PageCache::GetInstance()->Getmtx().lock();
    void* cur = start;
    while (cur)
    {
        void* next = NextObj(cur);
        Span* span = PageCache::GetInstance()->MapObjToSpan(cur);

        if (groupStart.find(span) == groupStart.end())
        {
            groupStart[span] = cur;
            groupEnd[span] = cur;
            groupCount[span] = 1;
        }
        else
        {
            NextObj(groupEnd[span]) = cur;
            groupEnd[span] = cur;
            groupCount[span] += 1;
        }
        cur = next;
    }
    // 断尾
    for (auto& kv : groupEnd)
    {
        NextObj(kv.second) = nullptr;
    }
    PageCache::GetInstance()->Getmtx().unlock();

    // 2) 在中心缓存桶锁下，批量把各组挂回span，并处理useCount==0的回收
    _spanList[index]._mtx.lock();

    for (auto& kv : groupStart)
    {
        Span* span = kv.first;
        void* gstart = kv.second;
        void* gend = groupEnd[span];
        size_t cnt = groupCount[span];

        NextObj(gend) = span->_freeList;
        span->_freeList = gstart;
        span->_useCount -= cnt;

        if (span->_useCount == 0)
        {
            _spanList[index].Erase(span);
            span->_freeList = nullptr;
            span->_next = nullptr;
            span->_prev = nullptr;

            _spanList[index]._mtx.unlock();

            PageCache::GetInstance()->Getmtx().lock();
            PageCache::GetInstance()->ReleaseSpanToPageCache(span);
            PageCache::GetInstance()->Getmtx().unlock();

            _spanList[index]._mtx.lock();
        }
    }

    _spanList[index]._mtx.unlock();
}
