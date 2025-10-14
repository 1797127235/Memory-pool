#include"ThreadCache.h"
#include"CentralCache.h"

//申请内存
void* ThreadCache::Allocate(size_t size) {
    assert(size <= MAX_BYTES); //小于256kb的内存申请才是有效

    size_t pos = SizeClass::Index(size);
    size_t alignsize = SizeClass::RoundUp(size);
    if (!_freeList[pos].Empty()) {
        return _freeList[pos].Pop();
    }
    else { //表示当前桶没有空闲的空间
        return FetchFromCentralCache(pos,alignsize);
    }
}

//向中心缓存申请内存
void* ThreadCache::FetchFromCentralCache(size_t index,size_t size) {
    assert(size <= MAX_BYTES);

    //慢开始算法
    size_t batchNum = std::min(_freeList[index].MaxSize(),SizeClass::NumMoveSize(size));
    size_t max_move = SizeClass::NumMoveSize(size);
    size_t cur_max = _freeList[index].MaxSize();
    //申请一段内存
    void* start = nullptr;
    void* end = nullptr;
    size_t n = CentralCache::GetInstance()->FetchRangeObj(start,end,batchNum,size);
    if (n == 0) {
        // 可选：对 MaxSize 做一次温和退让，避免在压力下无限增大（若之前增长过）
        if (cur_max > 1) {
            _freeList[index].SetMaxSize(cur_max - 1); // 或者衰减到 max(1, cur_max/2)
        }
        return nullptr; // 由上层走慢路径（例如直接向 PageHeap 要 span）
    }
    if (n == 1) {
        assert(start == end);
        return start;
    }

    _freeList[index].PushRange(NextObj(start),end,n - 1);
    if (n == batchNum && cur_max < max_move) {
        _freeList[index].SetMaxSize(std::min(cur_max + 1,max_move));
    }
    else if (n < batchNum && cur_max > 1) {
        //没拿满，做退让
        _freeList[index].SetMaxSize(cur_max - 1);
    }

    return start;
}


void ThreadCache::Deallocate(void* p,size_t size)
{
    assert(p);
    assert(size <= MAX_BYTES);

    size_t pos = SizeClass::Index(size);

    _freeList[pos].Push(p);
    
    if(_freeList[pos].Size() >= _freeList[pos].MaxSize())
    {
        ListTooLong(_freeList[pos],size);
    }
}


//将当前链表过长的内存归还给中心缓存
void ThreadCache::ListTooLong(FreeList& list,size_t size)
{
    void* start = nullptr;
    void* end = nullptr;
    list.PopRange(start,end,list.MaxSize());
    CentralCache::GetInstance()->ReleaseListToSpans(start,size);
}


