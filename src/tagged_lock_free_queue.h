#ifndef TAGGED_LOCK_FREE_QUEUE_H
#define TAGGED_LOCK_FREE_QUEUE_H

#include "abstract_queue.h"

#include <array>
#include <atomic>
#include <cstddef>

namespace lock_free {

template <typename T>
struct node;

template <typename T>
struct tagged_pointer
{
    node<T>* ptr;
    uintptr_t tag;

    tagged_pointer() noexcept: ptr(nullptr), tag(0) { }
    tagged_pointer(node<T>* p) noexcept: ptr(p), tag(0) { }
    tagged_pointer(node<T>* p, unsigned int n) noexcept: ptr(p), tag(n) { }
};

template <typename T>
struct node
{
    T data;
    std::atomic< tagged_pointer<T> > next;
};

template <typename T>
bool operator==(const tagged_pointer<T>& a, const tagged_pointer<T>& b)
{
    return a.ptr == b.ptr && a.tag == b.tag;
}

// lock-free queue using tagged pointers
template <typename T, size_t N = 100>
class tagged_lock_free_queue: public queue<T>
{
public:
    tagged_lock_free_queue()
    {
        for (size_t i = 0; i < N - 1; ++i)
            node_storage[i].next.store(tagged_pointer<T>(&node_storage[i+1]));

        node_storage[N - 1].next.store(tagged_pointer<T>());

        free_nodes.store(tagged_pointer<T>(&node_storage[0]));
        node<T>* new_node = get_free_node();
        new_node->next.store(tagged_pointer<T>());

        head.store(tagged_pointer<T>(new_node));
        tail.store(tagged_pointer<T>(new_node));
    }

    bool enqueue(const T& value) override
    {
        node<T>* new_node = get_free_node();
        if (new_node == nullptr)
            return false;
        new_node->data = value;
        new_node->next.store(tagged_pointer<T>());

        tagged_pointer<T> new_tail;

        for(;;)
        {
            new_tail = tail.load();
            tagged_pointer<T> next = new_tail.ptr->next.load();

            if (new_tail == tail.load())
            {
                if (next.ptr == nullptr)
                {
                    if (std::atomic_compare_exchange_strong(&new_tail.ptr->next,
                             &next, tagged_pointer<T>(new_node, next.tag + 1)))
                                            break;
                } else
                {
                    std::atomic_compare_exchange_strong(&tail, &new_tail,
                         tagged_pointer<T>(next.ptr, new_tail.tag + 1));
                }
            }
        }

        std::atomic_compare_exchange_strong(&tail,
             &new_tail, tagged_pointer<T>(new_node, new_tail.tag + 1));
        return true;
    }

    bool dequeue(T& result) override
    {
        tagged_pointer<T> new_head;

        for(;;)
        {
            new_head = head.load();
            tagged_pointer<T> new_tail = tail.load();
            tagged_pointer<T> next = new_head.ptr->next.load();

            if (new_head == head.load())
            {
                if (new_head.ptr == new_tail.ptr)
                {
                    if (next.ptr == nullptr)
                        return false;
                    std::atomic_compare_exchange_strong(&tail, &new_tail,
                         tagged_pointer<T>(next.ptr, new_tail.tag + 1));
                } else
                {
                result = std::move(next.ptr->data);
                if (std::atomic_compare_exchange_strong(&head, &new_head,
                         tagged_pointer<T>(next.ptr, new_head.tag + 1)))
                    break;
                }
            }
        }

        add_to_free_nodes(new_head.ptr);
        return true;
    }

protected:
    alignas(128) std::atomic< tagged_pointer<T> > head;
    alignas(128) std::atomic< tagged_pointer<T> > tail;
    alignas(128) std::atomic< tagged_pointer<T> > free_nodes;

    std::array<node<T>, N> node_storage;

    node<T>* get_free_node()
    {
        tagged_pointer<T> next;
        tagged_pointer<T> curr = free_nodes.load();

        do
        {
            if (curr.ptr == nullptr)
                return nullptr;
            next.tag = curr.tag + 1;
            next.ptr = curr.ptr->next.load().ptr;
        } while (!free_nodes.compare_exchange_weak(curr, next,
                                                   std::memory_order_acq_rel,
                                                   std::memory_order_relaxed));
        return curr.ptr;
    }

    void add_to_free_nodes(node<T>* node)
    {
        tagged_pointer<T> new_top;
        tagged_pointer<T> curr = free_nodes.load();

        do
        {
            node->next = curr.ptr;
            new_top.tag = curr.tag + 1;
            new_top.ptr = node;
        } while (!free_nodes.compare_exchange_weak(curr, new_top,
                                                   std::memory_order_acq_rel,
                                                   std::memory_order_relaxed));
    }
};

} // namespace lock_free

#endif // TAGGED_LOCK_FREE_QUEUE_H
