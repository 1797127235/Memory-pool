#pragma once
#include <mutex>
#include <atomic>
#include <new>
#include <cstddef>
#include <cassert>
#include <limits>
#include <stdexcept>


#ifdef _WIN32
  #define NOMINMAX
  #include <windows.h>
#else
  #include <sys/mman.h>
  #include <unistd.h>
  typedef uint64_t PAGE_ID;
  #ifndef MAP_ANONYMOUS
    #define MAP_ANONYMOUS MAP_ANON
  #endif
#endif

inline std::size_t os_page_size() {
#ifdef _WIN32
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return static_cast<std::size_t>(si.dwPageSize);
#else
    long p = ::sysconf(_SC_PAGESIZE);
    return p > 0 ? static_cast<std::size_t>(p) : 4096u;
#endif
}

inline bool safe_mul_bytes(std::size_t a, std::size_t b, std::size_t& out) {
    if (a == 0 || b == 0) { out = 0; return true; }
    if (a > std::numeric_limits<std::size_t>::max() / b) return false;
    out = a * b;
    return true;
}

// 直接向 OS 申请 kpage 页（kpage==0 => 1页）
inline void* SystemAlloc(std::size_t kpage) {
    if (kpage == 0) kpage = 1;
    const std::size_t ps = os_page_size();

    std::size_t bytes = 0;
    if (!safe_mul_bytes(kpage, ps, bytes) || bytes == 0) {
        throw std::bad_alloc();
    }

#ifdef _WIN32
    void* ptr = ::VirtualAlloc(nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!ptr) throw std::bad_alloc();
    return ptr;
#else
    void* ptr = ::mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ptr == MAP_FAILED) throw std::bad_alloc();
    return ptr;
#endif
}

// 归还 kpage 页（必须与分配时相同）
// Windows 忽略 size；Linux 需要精确的字节数
inline void SystemFree(void* ptr, std::size_t kpage) {
    if (!ptr) return;
#ifdef _WIN32
    ::VirtualFree(ptr, 0, MEM_RELEASE);
#else
    if (kpage == 0) {
        // 若你确实可能传不回页数，建议把“原始 bytes”保存在上层结构里并传回来。
        // 这里防御性地直接返回或抛异常。我们选择抛异常帮助尽早暴露 bug。
        throw std::invalid_argument("SystemFree: kpage must be the exact mapped page count on Linux");
    }
    const std::size_t ps = os_page_size();
    std::size_t bytes = 0;
    if (!safe_mul_bytes(kpage, ps, bytes) || bytes == 0) {
        throw std::invalid_argument("SystemFree: invalid kpage");
    }
    if (::munmap(ptr, bytes) != 0) {
        // 你也可以 perror，这里保持异常统一处理
        throw std::runtime_error("munmap failed");
    }
#endif
}

static void*& NextObj(void* obj)
{
    return *(void**)obj;
}


static const size_t PAGE_SHIFT = 12;

static const size_t MAX_BYTES = 256 * 1024;

static const size_t NFREELIST = 208;

static const size_t NPAGES = 129;



//管理ThreadCache的链表
class FreeList
{
private:
    void* _freeList = nullptr;
    //记录链表中对象的个数
    size_t  _size = 0;

    //一次申请
    size_t _cwnd = 1;

public:
    size_t GetCwnd()
    {
        return _cwnd;
    }

    void SetCwnd(size_t cwnd)
    {
        _cwnd = cwnd;
    }
    void Push(void* obj)
    {
        NextObj(obj) = _freeList;
        _freeList = obj;
        ++_size;
    }

    void PushRange(void* start, void* end, size_t n)
    {
        assert(start && end);
        if(n == 0) return ;

        NextObj(end) = _freeList;
        _freeList = start;
        _size += n;
    }

    void PopRange(void*& start, void*& end,size_t n)
    {
        assert(n <= _size);

        if(n == 0) return ;

        start = _freeList;

        end = start;

        for(size_t i = 0; i < n - 1; ++i)
        {
            end = NextObj(end);
        }

        _freeList = NextObj(end);
        NextObj(end) = nullptr;
        _size -= n;
    }

    void* Pop()
    {
        assert(_freeList);

        void* obj = _freeList;
        _freeList = NextObj(obj);
        --_size;

        return obj;
    }

    bool Empty() const {
        // 保证 _size 与 _freeList 一致
        assert((_freeList == nullptr) == (_size == 0));
        return _freeList == nullptr; // 用指针判空更直接
    }



    size_t GetSize() const
    {
        return _size;
    
    
    }

    size_t& Size() 
    {
        return _size;
    }
};


class SizeClass
{
public:
    static inline size_t _RoundUp(size_t bytes,size_t alignNum)
    {
        return ((bytes + alignNum - 1) & ~(alignNum - 1));
    }

    static inline size_t RoundUp(size_t size)
    {
        if (size <= 128)
        {
            return _RoundUp(size, 8);
        } else if (size <= 1024)
        {
            return _RoundUp(size, 16);
        }
        else if (size <= 8 * 1024)
        {
            return _RoundUp(size, 128);
        } 
        else if (size <= 64 * 1024)
        {
            return _RoundUp(size, 1024);
        }
        else if (size <= 256 * 1024)
        {
            return _RoundUp(size, 8 * 1024);
        }
        else
        {
            return _RoundUp(size, 1 << PAGE_SHIFT);
        }
    }



    static inline size_t _Index(size_t bytes, size_t align_shift)
	{
		return ((bytes + (1 << align_shift) - 1) >> align_shift) - 1;
	}

	// 计算映射的哪一个自由链表桶
	static inline size_t Index(size_t bytes)
	{
		assert(bytes <= MAX_BYTES);

		// 每个区间有多少个链
		static int group_array[4] = { 16, 56, 56, 56 };
		if (bytes <= 128){
			return _Index(bytes, 3);
		}
		else if (bytes <= 1024){
			return _Index(bytes - 128, 4) + group_array[0];
		}
		else if (bytes <= 8 * 1024){
			return _Index(bytes - 1024, 7) + group_array[1] + group_array[0];
		}
		else if (bytes <= 64 * 1024){
			return _Index(bytes - 8 * 1024, 10) + group_array[2] + group_array[1] + group_array[0];
		}
		else if (bytes <= 256 * 1024){
			return _Index(bytes - 64 * 1024, 13) + group_array[3] + group_array[2] + group_array[1] + group_array[0];
		}
		else{
			assert(false);
		}

		return -1;
	}



    // 一次thread cache从中心缓存获取多少个
	static size_t NumMoveSize(size_t size)
	{
		assert(size > 0);

		// [2, 512]，一次批量移动多少个对象的(慢启动)上限值
		// 小对象一次批量上限高
		// 小对象一次批量上限低
		int num = MAX_BYTES / size;
		if (num < 2)
			num = 2;

		if (num > 512)
			num = 512;

		return num;
	}



	// 计算一次向系统获取几个页
	// 单个对象 8byte
	// ...
	// 单个对象 256KB
	static size_t NumMovePage(size_t size)
	{
		size_t num = NumMoveSize(size);
		size_t npage = num*size;

		npage >>= PAGE_SHIFT;
		if (npage == 0)
			npage = 1;

		return npage;
	}
};


//管理连续页的大块内存
struct Span
{
    //页号
    PAGE_ID _page_id;
    //页数
    size_t _n;
    //双向链表
    Span* _next;
    Span* _prev;
    //管理的小对象大小
    size_t _objSize;
    //管理的小对象使用个数（原子类型防止并发问题）
    std::atomic<size_t> useCount{0};
    //切好的小对象链表
    void* _freeList = nullptr;
    //是否正在使用
    std::atomic<bool> _isUse{false};
};


class SpanList
{
private:
    Span* _head;
public:
    std::mutex _mtx;
public:
    SpanList()
    {
        _head = new Span;
        _head->_next = _head;
        _head->_prev = _head;
    }

    Span* Begin()
    {
        return _head -> _next;
    }

    Span* End()
    {
        return _head;
    }

    bool Empty()
    {
        return _head->_next == _head;
    }


    //pos前插入span
    void Insert(Span* pos,Span* span)
    {
        assert(pos);
        assert(span);

        Span* prev = pos->_prev;
        // prev newspan pos
        prev->_next = span;
        span->_prev = prev;
        span->_next = pos;
        pos->_prev = span;
    }

    void PushFront(Span* span)
    {
        Insert(_head->_next,span);
    }


    //并不是真的删除
    void Erase(Span* pos)
    {
        assert(pos);
        assert(pos != _head);

        if (!pos->_prev || !pos->_next) {
            return;
        }

        Span* prev = pos->_prev;
        Span* next = pos->_next;

        prev->_next = next;
        next->_prev = prev;

        pos->_prev = pos->_next = nullptr;
    }

    Span*Pop()
    {
        assert(!Empty());

        Span* span = _head->_next;
        Erase(span);
        return span;
    }
};



