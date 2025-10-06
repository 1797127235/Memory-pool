#include"ThreadCache.h""

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

//归还内存
void ThreadCache::Deallocate(void* p,size_t size) {
    size_t pos = SizeClass::Index(size);
    _freeList[pos].Push(p);
}
