#pragma once
#include "Common.hpp"
#include "CentralCache.hpp"
#include <array>
#include<algorithm>
#include <cstddef>
class ThreadCache
{
private:
    FreeList _freeList[NFREELIST];
    std::array<size_t,NFREELIST> _ssthresh{};
public:
    ThreadCache()
    {
        for(size_t i = 0; i < NFREELIST;++i)
        {
            _freeList[i].SetCwnd(1);
            _ssthresh[i] = 32;
        }
    }

    ~ThreadCache(){}

    void* Allocate(size_t size)
    {
        assert(size <= MAX_BYTES);

        //size_t index = SizeClass::Index(size);
        size_t csize = SizeClass::RoundUp(size);
        size_t index = SizeClass::Index(csize);

        if(!_freeList[index].Empty())
        {
            return _freeList[index].Pop();
        }
        else //空闲链表里没有多于对象了，向下层要
        {
            void* p = FetchFromCentralCache(index,size);
            return p;
        }
    }


    void Deallocate(void* p,size_t size)
    {
        assert(size <= MAX_BYTES);

        //size_t index = SizeClass::Index(size);
        size_t csize = SizeClass::RoundUp(size);
        size_t index = SizeClass::Index(csize);

        _freeList[index].Push(p);

        //当前链表过长，回收到中心缓存
        if(_freeList[index].Size() >= _freeList[index].GetCwnd())
        {
            ListTooLong(_freeList[index],size);
        }
    }

    //向中心缓存要批量对象 
    //返回一个对象给上层，剩余的全部挂在freelist
    //批量申请对象采用慢开始算法
    void* FetchFromCentralCache(size_t index,size_t size)
    {
        assert(size <= MAX_BYTES);

        static constexpr size_t kBudgeBytes = 64 * 1024;

        const size_t freeList_bytes = _freeList[index].Size() * size;

        const size_t room_objs = (freeList_bytes < kBudgeBytes) ? (kBudgeBytes - freeList_bytes) : 1;

        const size_t cap_by_size = SizeClass::NumMoveSize(size);

        size_t cwnd = _freeList[index].GetCwnd();
        size_t ssthresh = _ssthresh[index];


        size_t batchNum = std::max<size_t>(1, std::min({cwnd, cap_by_size, room_objs}));
        

        void* start = nullptr;
        void* end = nullptr;

        size_t n = CentralCache::GetInstance()->FetchRangeObj(start, end, size, batchNum);

        if (n == 0) {
            // 没拿到对象，控制拥塞参数即可
            _ssthresh[index] = std::max<size_t>(2, cwnd/2);
            _freeList[index].SetCwnd(1);
            return nullptr;
        }

        if (n == batchNum) {
            // 满载：加速增长
            if (cwnd < ssthresh) _freeList[index].SetCwnd(std::min(cwnd*2, cap_by_size));
            else                 _freeList[index].SetCwnd(std::min(cwnd+1, cap_by_size));
        } else {
            // 未满：慢启动/拥塞避免
            _ssthresh[index] = std::max<size_t>(2, cwnd/2);
            _freeList[index].SetCwnd(1);
        }

        // 返回一个，剩下的挂到本地 freelist
        if (n == 1) return start;
        _freeList[index].PushRange(NextObj(start), end, n - 1);
        return start;
    }

    //链表过长归还内存
    void ListTooLong(FreeList& list, size_t size)
    {
        //找到对应的中心缓存链表，挂接到中心缓存
        size_t csize = SizeClass::RoundUp(size);
        size_t index = SizeClass::Index(csize);

        void* start = nullptr;
        void* end = nullptr;

        //归还一半
        list.PopRange(start,end,list.GetCwnd() >> 1);

        //归还到中心缓存
        CentralCache::GetInstance()->ReleaseListToSpans(start,size);

    }
};


struct ThreadCacheHolder {
    ThreadCache* ptr = nullptr;
    ~ThreadCacheHolder() { delete ptr; }
};

inline ThreadCache*& GetThreadCache() {
    thread_local ThreadCacheHolder holder;
    return holder.ptr;
}

#define g_threadCache GetThreadCache()