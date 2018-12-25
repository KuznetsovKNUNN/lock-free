#ifndef HAZARD_LOCK_FREE_QUEUE_H
#define HAZARD_LOCK_FREE_QUEUE_H

#include "abstract_queue.h"
#include "hazard_pointer.h"

#include <atomic>
#include <memory>

namespace lock_free {

// lock-free очередь с использованием опасных указателей (hazard pointers)
template <typename T>
class hazard_lock_free_queue : public queue<T>
{
public:
    hazard_lock_free_queue()
    {
        node* p = new node();

        // queue_head и queue_tail указывают на dummy node
        // очередь пустая когда head == tail и tail->next == nullptr
        queue_head.store(p);
        queue_tail.store(p);
    }

    bool enqueue(const T& value) override
    {
        node* new_node = new node();
        new_node->data = value;

        node* tail;
        while (true)
        {
            std::atomic<void*>& hp = get_hazard_pointer_for_current_thread(0);
            tail = queue_tail.load();
            // объявляем tail как hazard указатель
            hp.store(tail);
            // проверяем что tail не изменился
            if (tail != queue_tail.load()) continue;

            node* next = tail->next.load();
            if (tail != queue_tail.load()) continue;

            if (next != nullptr)
            {
                // queue_tail указывает не на последний элемент
                queue_tail.compare_exchange_weak(tail, next);
                continue;
            }

            node* temp = nullptr;
            // записываем new_node в tail->next
            // при условии что tail->next == nullptr
            if (tail->next.compare_exchange_strong(temp, new_node))
                          break;
        }

        // пробуем переместить queue_tail на вставленный элемент
        queue_tail.compare_exchange_strong(tail, new_node);
        get_hazard_pointer_for_current_thread(0).store(nullptr);
        return true;
    }

    bool dequeue(T& result) override
    {
        node* head;

        while (true)
        {
            // объявляем head и head->next как hazard
            std::atomic<void*>& hp1 = get_hazard_pointer_for_current_thread(0);
            std::atomic<void*>& hp2 = get_hazard_pointer_for_current_thread(1);
            head = queue_head.load();
            hp1.store(head);
            if (head != queue_head.load()) continue;

            node* tail = queue_tail.load();
            node* next = head->next.load();
            hp2.store(next);
            if (head != queue_head.load()) continue;

            if (next == nullptr)
            {
                // пустая очередь
                hp1.store(nullptr);
                return false;
            }

            if (head == tail)
            {
                // queue_tail указывает не на последний  элемент
                queue_tail.compare_exchange_strong(tail, next);
                continue;
            }

            result = next->data;
            // пытаемся передвинуть queue_head на head->next
            if (queue_head.compare_exchange_strong(head, next)) break;
        }

        // обнуляем hazard указатели
        get_hazard_pointer_for_current_thread(0).store(nullptr);
        get_hazard_pointer_for_current_thread(1).store(nullptr);

        // добавляем dummy node в reclaim_list
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
