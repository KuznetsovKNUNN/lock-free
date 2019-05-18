#ifndef TAGGED_LOCK_FREE_QUEUE_H
#define TAGGED_LOCK_FREE_QUEUE_H

#include "abstract_queue.h"

#include <array>
#include <atomic>
#include <cstddef>

namespace lock_free {

template <typename T>
struct node;

// для решения ABA-проблемы
// увеличиваем tag каждый раз при работе с указателем
template <typename T>
struct tagged_pointer
{
    node<T>* ptr;
    uintptr_t tag;

    tagged_pointer() noexcept: ptr(nullptr), tag(0) { }
    tagged_pointer(node<T>* p) noexcept: ptr(p), tag(0) { }
    tagged_pointer(node<T>* p, uintptr_t n) noexcept: ptr(p), tag(n) { }
};

template <typename T>
struct node
{
    T data;
    std::atomic<tagged_pointer<T>> next;
};

template <typename T>
bool operator==(const tagged_pointer<T>& a, const tagged_pointer<T>& b)
{
    return a.ptr == b.ptr && a.tag == b.tag;
}

// lock-free очередь с использованием меченых указателей (tagged pointers)
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

        // queue_head и queue_tail указывают на dummy node
        // очередь пустая когда head == tail и tail->next == nullptr
        node<T>* new_node = get_free_node();
        new_node->next.store(tagged_pointer<T>());
        queue_head.store(tagged_pointer<T>(new_node));
        queue_tail.store(tagged_pointer<T>(new_node));
    }

    bool enqueue(const T& value) override
    {
        node<T>* new_node = get_free_node();
        if (new_node == nullptr)
            return false;
        new_node->data = value;
        new_node->next.store(tagged_pointer<T>());

        tagged_pointer<T> tail;

        while (true)
        {
            tail = queue_tail.load();
            tagged_pointer<T> next = tail.ptr->next.load();

            if (tail == queue_tail.load())
            {
                // проверяем что tail указывает на последний элемент
                if (next.ptr == nullptr)
                {
                    // пробуем добавить элемент в конец списка
                    if (std::atomic_compare_exchange_strong(&tail.ptr->next,
                             &next, tagged_pointer<T>(new_node, next.tag + 1)))
                        break;
                } else
                {
                    // queue_tail не указывает на последний элемент
                    // пробуем переместить queue_tail
                    std::atomic_compare_exchange_strong(&queue_tail, &tail,
                         tagged_pointer<T>(next.ptr, tail.tag + 1));
                }
            }
        }

        // пробуем переместить queue_tail на вставленный элемент
        std::atomic_compare_exchange_strong(&queue_tail,
             &tail,tagged_pointer<T>(new_node, tail.tag + 1));
        return true;
    }

    bool dequeue(T& result) override
    {
        tagged_pointer<T> head;

        while (true)
        {
            head = queue_head.load();
            tagged_pointer<T> tail = queue_tail.load();
            tagged_pointer<T> next = head.ptr->next.load();

            if (head == queue_head.load())
            {
                // проверяем что очередь пуста или tail не последний
                if (head.ptr == tail.ptr)
                {
                    // проверяем что очередь пуста
                    if (next.ptr == nullptr)
                        return false; // // очередь пуста

                    // queue_tail не указывает на последний элемент
                    // пробуем переместить queue_tail
                    std::atomic_compare_exchange_strong(&queue_tail, &tail,
                         tagged_pointer<T>(next.ptr, tail.tag + 1));
                } else
                {
                    // очередь не пуста
                    result = next.ptr->data;
                    // пробуем передвинуть queue_head
                    if (std::atomic_compare_exchange_strong(&queue_head, &head,
                         tagged_pointer<T>(next.ptr, head.tag + 1)))
                        break;
                }
            }
        }

        // добавляем dummy node в список свободных элементов
        add_to_free_nodes(head.ptr);
        return true;
    }

protected:
    alignas(128) std::atomic<tagged_pointer<T>> queue_head;
    alignas(128) std::atomic<tagged_pointer<T>> queue_tail;

    // free_nodes указывает на свободные элементы в node_storage
    alignas(128) std::atomic<tagged_pointer<T>> free_nodes;

    // список свободных элементов
    // вместо удаления помещаем элемент в node_storage
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
        } while (!free_nodes.compare_exchange_weak(curr, next));
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
        } while (!free_nodes.compare_exchange_weak(curr, new_top));
    }
};

} // namespace lock_free

#endif // TAGGED_LOCK_FREE_QUEUE_H
