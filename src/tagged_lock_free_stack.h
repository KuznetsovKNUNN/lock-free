#ifndef TAGGED_LOCK_FREE_STACK_H
#define TAGGED_LOCK_FREE_STACK_H

#include "abstract_stack.h"

#include <array>
#include <atomic>
#include <cstddef>

namespace lock_free {

// lock-free stack using tagged pointers
template <typename T, size_t N = 100>
class tagged_lock_free_stack: public stack<T>
{
public:
    tagged_lock_free_stack()
    {
        head.store(tagged_pointer(), std::memory_order_relaxed);

        for (size_t i = 0; i < N - 1; ++i)
            node_storage[i].next.ptr = &node_storage[i + 1];
        node_storage[N - 1].next = tagged_pointer();

        free_nodes.store(tagged_pointer(&node_storage[0]),
                std::memory_order_relaxed);
    }

    bool push(const T& value) override
    {
        node* new_node = get(free_nodes);
        if (new_node == nullptr)
            return false;
        new_node->data = value;
        put(head, new_node);
        return true;
    }

    bool pop(T& result) override
    {
        node* node = get(head);
        if (node == nullptr)
            return false;
        result = std::move(node->data);
        put(free_nodes, node);
        return true;
    }

protected:
    struct node;

    struct tagged_pointer
    {
        node* ptr;
        uintptr_t tag;

        tagged_pointer() noexcept: ptr(nullptr), tag(0) { }
        tagged_pointer(node* p) noexcept: ptr(p), tag(0) { }
        tagged_pointer(node* p, unsigned int n) noexcept: ptr(p), tag(n) { }
    };

    struct node
    {
        T data;
        tagged_pointer next;
    };

    alignas(128) std::atomic<tagged_pointer> head;
    alignas(128) std::atomic<tagged_pointer> free_nodes;

    std::array<node, N> node_storage;

    node* get(std::atomic<tagged_pointer>& top)
    {
        tagged_pointer next;
        tagged_pointer curr = top.load(std::memory_order_relaxed);

        do
        {
            if (curr.ptr == nullptr)
                return nullptr;
            next.tag = curr.tag + 1;
            next.ptr = curr.ptr->next.ptr;
        } while (!top.compare_exchange_weak(curr, next,
                                            std::memory_order_acq_rel,
                                            std::memory_order_relaxed));
        return curr.ptr;
    }

    void put(std::atomic<tagged_pointer>& top, node* node)
    {
        tagged_pointer new_top;
        tagged_pointer curr = top.load(std::memory_order_relaxed);

        do
        {
            node->next = curr.ptr;
            new_top.tag = curr.tag + 1;
            new_top.ptr = node;
        } while (!top.compare_exchange_weak(curr, new_top,
                                            std::memory_order_acq_rel,
                                            std::memory_order_relaxed));
    }
};

} // namespace lock_free

#endif // TAGGED_LOCK_FREE_STACK_H
