#include "CentralCache.h"

CentralCache CentralCache::_sInst;

Span *CentralCache::GetOneSpan(SpanList &list, size_t byte_size) {

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
    span->_useCount += actualNum;

    _spanList[index]._mtx.unlock();

    return actualNum;

}
