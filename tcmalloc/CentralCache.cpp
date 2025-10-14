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
    int i = 0;
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

    _spanList[index]._mtx.unlock();

    return actualNum;

}


void CentralCache::ReleaseListToSpans(void* start, size_t size) 
{
    int index = SizeClass::Index(size);

    _spanList[index]._mtx.lock();

    while(start)
    {
        void* next = NextObj(start);
        //看是哪页的span
        Span* span = PageCache::GetInstance()->MapObjToSpan(start);
        
        NextObj(start) = span->_freeList;
        span->_freeList = start;
        span->_useCount--;

        if(span->_useCount == 0) //span使用计数为0，归还给pagecache，合并大页
        {
            _spanList[index].Erase(span);
            span->_freeList = nullptr;
            span->_next = nullptr;
            span->_prev = nullptr;
            
            _spanList[index]._mtx.unlock();

            PageCache::GetInstance()-> Getmtx().lock();

            PageCache::GetInstance()-> ReleaseSpanToPageCache(span);

            PageCache::GetInstance()-> Getmtx().unlock();

            _spanList[index]._mtx.lock();
        }

        start = next;
    }

    _spanList[index]._mtx.unlock();
}
