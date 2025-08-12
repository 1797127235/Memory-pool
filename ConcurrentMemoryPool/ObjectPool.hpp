#pragma once
#include<iostream>
#include<vector>
#include<cstdlib>
#include<new>
using std::cout;
using std::endl;
const int DEFAULT_SIZE=128*1024; //128k
template<class T>
class ObjectPool
{
public:
    ObjectPool():
    _memory(nullptr),
    _remainBytes(0),
    _freeList(nullptr)
    {}

    ~ObjectPool()
    {
        for(void*blk:_chunks) std::free(blk);
        _chunks.clear();
    }

    T*New()
    {
        if(_freeList)
        {
            void *node =_freeList;
            _freeList=*reinterpret_cast<void**>(_freeList);
            return reinterpret_cast<T*>(node);
        }
        //内存不足申请一块 
        if(_remainBytes<sizeof(T)) 
        {
            void * blk=std::malloc(DEFAULT_SIZE);
            if(!blk) throw std::bad_alloc();
            _chunks.push_back(blk);
            _memory=reinterpret_cast<char*>(blk);
            _remainBytes=DEFAULT_SIZE;
        }
        //从大块切一段
        T*obj=(T*)_memory;
        //
        size_t objSize=sizeof(T)<sizeof(void*) ? sizeof(void*) : sizeof(T);
        _memory+=objSize;
        _remainBytes-=objSize;

        // //定位new 显示调用T的构造 在已经分配的内存上调用构造函数
        // new(obj)T();
        return obj;
    }

    void Delete(T* obj) {
        //obj->~T();
        // 头插：把 obj 的起始处当作 "next" 指针存放
        *reinterpret_cast<void**>(obj) = _freeList;
        _freeList = obj;
    }

    template<typename... Args>
    T* Construct(Args&&... args)
    {
        void * mem=New();
        return new(mem)T(std::forward<Args>(args)...);
    }

    void Destroy(T* obj)
    {
        if(obj) obj->~T();
        Delete(obj);
    }

    ObjectPool(const ObjectPool&) = delete;
    ObjectPool& operator=(const ObjectPool&) = delete;



private:
    char * _memory=nullptr; //当前大块内存
    size_t _remainBytes=0; //当前大块内存剩余字节数
    void * _freeList=nullptr; //空闲链表

    std::vector<void*> _chunks;//所有大块指针,析构时统一释放
};