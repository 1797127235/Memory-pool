#include "CentralCache.h"

CentralCache CentralCache::_sInst;


size_t CentralCache::FetchRangeObj(void *&start, void *&end, size_t batchNum, size_t size) {
    //先找到对应的桶
    size_t index = SizeClass::Index(size);

    if (!_spanList[index].Empty()) {

    }

}
