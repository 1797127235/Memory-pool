#pragma once
#include "Common.h"

// 定长内存池 - 为特定类型对象提供高效的内存分配和回收
// 设计目标:
// 1. 减少频繁 new/delete 的开销
// 2. 提高内存局部性,减少缓存未命中
// 3. 避免内存碎片化
// 主要用于 tcmalloc 内部频繁分配的 Span 对象

template<class T>
class ObjectPool
{
public:
	T* New()
	{
		T* obj = nullptr;

		// 优先从自由链表中获取已回收的对象,提高内存复用率
		if (_freeList)
		{
			obj = (T*)_freeList;
			_freeList = NextObj(_freeList);
			++_allocCount;
		}
		else
		{
			// 剩余内存不够一个对象大小时,重新申请大块内存
			if (_remainBytes < sizeof(T))
			{
				// 根据对象大小动态调整分配块大小
				// 小对象(< 256B): 128KB
				// 中等对象(256B-4KB): 256KB  
				// 大对象(> 4KB): 512KB
				if (sizeof(T) <= 256)
				{
					_remainBytes = SMALL_BLOCK_SIZE;
				}
				else if (sizeof(T) <= 4096)
				{
					_remainBytes = MEDIUM_BLOCK_SIZE;
				}
				else
				{
					_remainBytes = LARGE_BLOCK_SIZE;
				}

				_memory = (char*)SystemAlloc(_remainBytes >> PAGE_SHIFT);
				if (_memory == nullptr)
				{
					throw std::bad_alloc();
				}
				++_blockCount;
			}

			obj = (T*)_memory;
			// 确保对象大小至少能存放一个指针(用于自由链表)
			size_t objSize = sizeof(T) < sizeof(void*) ? sizeof(void*) : sizeof(T);
			_memory += objSize;
			_remainBytes -= objSize;
			++_allocCount;
		}

		// 使用 placement new 调用构造函数
		// 注意:如果构造函数抛异常,对象已从内存池分配,但未初始化
		// 这里选择让异常传播,由调用者处理
		new(obj)T();

		return obj;
	}

	void Delete(T* obj)
	{
		if (obj == nullptr)
		{
			return;
		}

		// 显式调用析构函数清理对象
		obj->~T();

		// 将对象插入自由链表头部(O(1)操作)
		NextObj(obj) = _freeList;
		_freeList = obj;
		++_freeCount;
	}

	// 获取统计信息 - 用于性能分析和调试
	size_t GetAllocCount() const { return _allocCount; }
	size_t GetFreeCount() const { return _freeCount; }
	size_t GetBlockCount() const { return _blockCount; }

 private:
	// 内存分配策略常量
	static const size_t SMALL_BLOCK_SIZE = 128 * 1024;   // 128KB
	static const size_t MEDIUM_BLOCK_SIZE = 256 * 1024;  // 256KB
	static const size_t LARGE_BLOCK_SIZE = 512 * 1024;   // 512KB

	char* _memory = nullptr;      // 指向当前大块内存的指针
	size_t _remainBytes = 0;      // 当前大块内存的剩余字节数
	void* _freeList = nullptr;    // 自由链表头指针(已回收对象链表)

	// 统计信息
	size_t _allocCount = 0;       // 累计分配次数
	size_t _freeCount = 0;        // 累计释放次数  
	size_t _blockCount = 0;       // 申请的大块内存数量
};
