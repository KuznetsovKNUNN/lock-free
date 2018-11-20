#ifndef HAZARD_LOCK_FREE_STACK_H
#define HAZARD_LOCK_FREE_STACK_H

#include "abstract_stack.h"
#include "hazard_pointer.h"

#include <atomic>
#include <memory>

namespace lock_free {

// lock-free stack using hazard pointers
template <typename T>
class hazard_lock_free_stack: public stack<T>
{
public:
    hazard_lock_free_stack()
    {
        head.store(nullptr, std::memory_order_relaxed);
    }

    bool push(const T& value) override
    {
        node* new_node = new node();
        new_node->data = value;
        new_node->next = head.load();
        while (!head.compare_exchange_weak(new_node->next, new_node,
                                           std::memory_order_acq_rel,
                                           std::memory_order_relaxed));
        return true;
    }

    bool pop(T& result) override
    {
        std::atomic<void*>& hp = get_hazard_pointer_for_current_thread();
        node *old_head = head.load();
        do
        {
            node* temp;
            do
            {
                temp = old_head;
                hp.store(old_head);
                old_head = head.load();
            } while (old_head != temp);
        } while (old_head &&
                !head.compare_exchange_strong(old_head, old_head->next,
                                              std::memory_order_acq_rel,
                                              std::memory_order_relaxed));

        hp.store(nullptr);
        if (old_head)
        {
            result = std::move(old_head->data);
            if (hazard(old_head))
                reclaim_later(old_head);
            else
                delete old_head;

            delete_nodes_with_no_hazards();
            return true;
        }
        return false;
    }

protected:
    struct node
    {
        T data;
        node* next;
    };

    std::atomic<node*> head;
};

} // namespace lock_free

#endif // HAZARD_LOCK_FREE_STACK_H
