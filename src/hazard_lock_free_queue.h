#ifndef HAZARD_LOCK_FREE_QUEUE_H
#define HAZARD_LOCK_FREE_QUEUE_H

#include "abstract_queue.h"
#include "hazard_pointer.h"

#include <atomic>
#include <memory>

namespace lock_free {

template <typename T>
class hazard_lock_free_queue : public queue<T>
{
public:
    hazard_lock_free_queue()
    {
        node* p = new node();

        queue_head.store(p, std::memory_order_release);
        queue_tail.store(p, std::memory_order_release);
    }

    bool enqueue(const T& value) override
    {
        node* new_node = new node();
        new_node->data = value;

        node* tail;

        for(;;)
        {
            std::atomic<void*>& hp = get_hazard_pointer_for_current_thread(0);
            node* tail = queue_tail.load(std::memory_order_relaxed);

            hp.store(tail);
            if (tail != queue_tail.load(std::memory_order_acquire)) continue;
            node* next = tail->next.load();
            if (tail != queue_tail.load(std::memory_order_acquire)) continue;

            if (next != nullptr)
            {
                queue_tail.compare_exchange_weak(tail, next,
                                                 std::memory_order_release);
                continue;
            }

            node* temp = nullptr;
            if (tail->next.compare_exchange_strong(temp, new_node,
                                                   std::memory_order_release))
                          break;
        }

        queue_tail.compare_exchange_strong(tail, new_node,
                                           std::memory_order_acq_rel);
        get_hazard_pointer_for_current_thread(0).store(nullptr);
        return true;
    }

    bool dequeue(T& result) override
    {
        node* head;

        for (;;)
        {
            std::atomic<void*>& hp1 = get_hazard_pointer_for_current_thread(0);
            std::atomic<void*>& hp2 = get_hazard_pointer_for_current_thread(1);
            head = queue_head.load(std::memory_order_relaxed);

            hp1.store(head);
            if (head != queue_head.load(std::memory_order_acquire)) continue;

            node* tail = queue_tail.load(std::memory_order_relaxed);
            node* next = head->next.load(std::memory_order_acquire);
            hp2.store(next);
            if (head != queue_head.load(std::memory_order_relaxed)) continue;

            if (next == nullptr)
            {
                hp1.store(nullptr);
                return false;
            }

            if (head == tail)
            {
                queue_tail.compare_exchange_strong(tail, next,
                                                   std::memory_order_release);
                continue;
            }

            result = std::move(next->data);
            if (queue_head.compare_exchange_strong(head, next,
                                                   std::memory_order_release))
                break;
        }

        get_hazard_pointer_for_current_thread(0).store(nullptr);
        get_hazard_pointer_for_current_thread(1).store(nullptr);

        reclaim_later(head);
        return true;
    }

protected:
    struct node
    {
        T data;
        std::atomic<node*> next;
    };

    std::atomic<node*> queue_head;
    std::atomic<node*> queue_tail;
};

} // namespace lock_free

#endif // HAZARD_LOCK_FREE_QUEUE_H
