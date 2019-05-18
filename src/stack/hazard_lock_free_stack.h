#ifndef HAZARD_LOCK_FREE_STACK_H
#define HAZARD_LOCK_FREE_STACK_H

#include "abstract_stack.h"
#include "hazard_pointer.h"

#include <atomic>
#include <memory>

namespace lock_free {

// lock-free стек с использованием опасных указателей (hazard pointers)
template <typename T>
class hazard_lock_free_stack: public stack<T>
{
public:
    hazard_lock_free_stack()
    {
        stack_head.store(nullptr);
    }

    bool push(const T& value) override
    {
        node* new_node = new node();
        new_node->data = value;
        new_node->next = stack_head.load();
        // передвигаем stack_head на new_node
        while (!stack_head.compare_exchange_weak(new_node->next, new_node));
        return true;
    }

    bool pop(T& result) override
    {
        std::atomic<void*>& hp = get_hazard_pointer_for_current_thread(0);

        node* head = stack_head.load();
        do
        {
            node* temp;
            do
            {
                temp = head;
                // отмечаем head как hazard
                hp.store(head);
                head = stack_head.load();
            } while (head != temp);
        } while (head && !stack_head.compare_exchange_strong(head, head->next));

        // stack_head передвинули на head->next
        // можно обнулить hazard указатель
        hp.store(nullptr);
        if (head)
        {
            result = head->data;
            reclaim_later(head);

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

    std::atomic<node*> stack_head;
};

} // namespace lock_free

#endif // HAZARD_LOCK_FREE_STACK_H
