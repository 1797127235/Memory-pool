#pragma once

#include<atomic>
#include<cstdint>
#include<new>

struct Span; // 前置声明

// 三层基数树，每层 12bit（4096 槽），覆盖 36bit 页号。
// 读路径完全无锁（atomic acquire-load），
// 写路径需在 PageCache 锁内调用（release-store 发布）。
class RadixSpanMap {
    static constexpr std::size_t BITS  = 12;
    static constexpr std::size_t MASK  = (1u << BITS) - 1;
    static constexpr std::size_t FANOUT = (1u << BITS);

    struct Leaf {
        std::atomic<Span*> slot[FANOUT];
        Leaf() noexcept {
            for (std::size_t i = 0; i < FANOUT; ++i)
                slot[i].store(nullptr, std::memory_order_relaxed);
        }
    };
    struct L2 {
        std::atomic<Leaf*> next[FANOUT];
        L2() noexcept {
            for (std::size_t i = 0; i < FANOUT; ++i)
                next[i].store(nullptr, std::memory_order_relaxed);
        }
    };
    struct Root {
        std::atomic<L2*> next[FANOUT];
        Root() noexcept {
            for (std::size_t i = 0; i < FANOUT; ++i)
                next[i].store(nullptr, std::memory_order_relaxed);
        }
    };

public:
    RadixSpanMap() = default;
    ~RadixSpanMap() = default; // 第一阶段不回收节点，避免读无锁时悬空

    // 读：完全无锁
    inline Span* Lookup(uint64_t page_id) const noexcept {
        const std::size_t i1 = (page_id >> 24) & MASK;
        const std::size_t i2 = (page_id >> 12) & MASK;
        const std::size_t i3 =  page_id         & MASK;

        L2* l2 = _root.next[i1].load(std::memory_order_acquire);
        if (!l2) return nullptr;
        Leaf* lf = l2->next[i2].load(std::memory_order_acquire);
        if (!lf) return nullptr;
        return lf->slot[i3].load(std::memory_order_acquire);
    }

    // 写：在 PageCache 的锁内批量调用
    inline void MapRange(uint64_t start_page, std::size_t n, Span* sp) {
        while (n--) MapOne(start_page++, sp);
    }
    inline void UnmapRange(uint64_t start_page, std::size_t n) {
        while (n--) MapOne(start_page++, nullptr);
    }

private:
    inline void MapOne(uint64_t page_id, Span* sp) {
        const std::size_t i1 = (page_id >> 24) & MASK;
        const std::size_t i2 = (page_id >> 12) & MASK;
        const std::size_t i3 =  page_id         & MASK;

        // L2
        // 安装 L2
        L2* l2 = _root.next[i1].load(std::memory_order_acquire);
        if (!l2) {
            L2* expect = nullptr;
            L2* neo = new L2();
            if (!_root.next[i1].compare_exchange_strong(expect, neo,
                    std::memory_order_release, std::memory_order_acquire)) {
                delete neo;
            }
            l2 = _root.next[i1].load(std::memory_order_acquire);
        }
        // 安装 Leaf
        Leaf* lf = l2->next[i2].load(std::memory_order_acquire);
        if (!lf) {
            Leaf* expect = nullptr;
            Leaf* neo = new Leaf();
            if (!l2->next[i2].compare_exchange_strong(expect, neo,
                    std::memory_order_release, std::memory_order_acquire)) {
                delete neo;
            }
            lf = l2->next[i2].load(std::memory_order_acquire);
        }
        // 槽直接 store，写路径保证 release 语义
        lf->slot[i3].store(sp, std::memory_order_release);
    }

private:
    Root _root;
};