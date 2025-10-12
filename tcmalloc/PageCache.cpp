#include "PageCache.h"

PageCache PageCache::_sInst;

//获取一个k页的span
Span *PageCache::NewSpan(size_t k) {
    assert(k > 0);
}
