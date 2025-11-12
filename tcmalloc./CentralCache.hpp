#pragma once
#include "Common.hpp"
#include"PageCache.hpp"

class CentralCache
{
private:
    SpanList _spanlist[NFREELIST];

    CentralCache() = default;
    ~CentralCache() = default;

    CentralCache(const CentralCache&) = delete;
    CentralCache& operator=(const CentralCache&) = delete;
    CentralCache(CentralCache&&) = delete;
    CentralCache& operator=(CentralCache&&) = delete;

    static  CentralCache _sInst;

    //申请一个Span
    Span* GetOneSpan(SpanList& list,size_t byte_size)
    {
        // 先在当前桶中查找已有可用 span
        Span* it = list.Begin();
        while (it != list.End())
        {
            if (it->_freeList)
            {
                return it;
            }
            it = it->_next;
        }

        // 走到这里，说明该桶下现有 span 都没有可用对象
        // 释放中心缓存桶锁，向 PageCache 申请新 span
        list._mtx.unlock();
        PageCache::GetInstance()->_mtx.lock();

        Span* span = PageCache::GetInstance()->NewSpan(SizeClass::NumMovePage(byte_size));
        span->_isUse = true;
        span->_objSize = byte_size;

        PageCache::GetInstance()->_mtx.unlock();

        // 对 span 进行切分，构建对象链表（保证指针对齐）
        char* base = reinterpret_cast<char*>(static_cast<uintptr_t>(span->_page_id) << PAGE_SHIFT);
        const size_t bytes = span->_n << PAGE_SHIFT;
        char* end = base + bytes;

        void* head = base;
        void* tail = head;
        char* cur = base + byte_size;
        while (cur + byte_size <= end)
        {
            NextObj(tail) = cur;
            tail = cur;
            cur += byte_size;
        }
        NextObj(tail) = nullptr;
        span->_freeList = head;

        // 重新加回中心缓存桶锁，把新 span 放回桶头
        list._mtx.lock();
        list.PushFront(span);
        return span;
    }

public:
    static CentralCache* GetInstance()
    {
        return &_sInst;
    }

    size_t FetchRangeObj(void*& start, void*& end, size_t size, size_t num)
    {
        const size_t byte_size = SizeClass::RoundUp(size);
        size_t index = SizeClass::Index(byte_size);

        // 手动加锁，配合 GetOneSpan 内部的解锁/加锁流程
        _spanlist[index]._mtx.lock();

        Span* span = GetOneSpan(_spanlist[index], byte_size);
        
        if (span == nullptr || span->_freeList == nullptr)
        {
            _spanlist[index]._mtx.unlock();
            start = end = nullptr;
            return 0;
        }

        start = span->_freeList;
        end = start;
        size_t moved = 1;
        while (moved < num && NextObj(end))
        {
            end = NextObj(end);
            ++moved;
        }

        // 从 span 的自由链中摘掉 [start, end]
        span->_freeList = NextObj(end);
        NextObj(end) = nullptr;
        
        //当前span使用个数+moved
        span->useCount.fetch_add(moved, std::memory_order_relaxed);
        span->_isUse = true;
        //对象大小
        span->_objSize = size;

        _spanlist[index]._mtx.unlock();
        return moved;
    }

    //将对象链表挂到span,当span的usecount = 0 的时候归还给pagecache
    void ReleaseListToSpans(void* start, size_t size)
    {
        size_t index = SizeClass::Index(size);
        _spanlist[index]._mtx.lock();
        while (start)
        {
            void* next = NextObj(start);

            Span* span = PageCache::GetInstance()->MapObjectToSpan(start);
            NextObj(start) = span->_freeList;
            span->_freeList = start;
            span->useCount.fetch_sub(1, std::memory_order_relaxed);

            // 说明span的切分出去的所有小块内存都回来了
            // 这个span就可以再回收给page cache，pagecache可以再尝试去做前后页的合并
            if (span->useCount.load(std::memory_order_relaxed) == 0)
            {
                _spanlist[index].Erase(span);
                span->_freeList = nullptr;
                span->_next = nullptr;
                span->_prev = nullptr;

                // 释放span给page cache时，使用page cache的锁就可以了
                // 这时把桶锁解掉
                _spanlist[index]._mtx.unlock();

                PageCache::GetInstance()->_mtx.lock();
                PageCache::GetInstance()->ReleaseSpanToPageCache(span);
                PageCache::GetInstance()->_mtx.unlock();

                _spanlist[index]._mtx.lock();
            }

            start = next;
        }

        _spanlist[index]._mtx.unlock();
    }

};

// 这样即使被多个翻译单元包含，也只有一个实体（不违反 ODR）
inline CentralCache CentralCache::_sInst{};
