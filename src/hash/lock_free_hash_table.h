#ifndef LOCK_FREE_HASH_TABLE_H
#define LOCK_FREE_HASH_TABLE_H

#include "hazard_pointer.h"
#include "hash.h"

#include <atomic>
#include <iostream>

namespace lock_free {

template <typename K, typename T, typename H>
class lock_free_hash_table
{
protected:
    struct node;
    using marked_ptr = node*;
    size_t buckets;

    struct node
    {
        K key;
        T data;
        std::atomic<marked_ptr> next;

        node(K k): key(k) { }
        node(K k, T val): key(k), data(val) { }
    };

    // marked ptr operations
    static uintptr_t get_bit(marked_ptr p)
    {
        return reinterpret_cast<uintptr_t>(p) & 1;
    }

    static marked_ptr set_bit(marked_ptr p, uintptr_t bit)
    {
        return reinterpret_cast<marked_ptr>(
                    (reinterpret_cast<uintptr_t>(p)) | bit);
    }

    static marked_ptr get_ptr(marked_ptr p)
    {
        return reinterpret_cast<marked_ptr>((reinterpret_cast<uintptr_t>(p))
                                            & ~(static_cast<uintptr_t>(1)));
    }

public:
    // lock-free ordered lists
    std::atomic<marked_ptr>* table;

    lock_free_hash_table(size_t mb = max_buckets)
    {
        buckets = mb;
        table = new std::atomic<marked_ptr>[buckets];
        for (size_t i = 0; i < buckets; ++i)
            table[i] = nullptr;
    }

    ~lock_free_hash_table()
    {
        if (table != nullptr)
            delete[] table;
    }

    // hash table operaions
    bool hash_insert(K key, const T& value)
    {
        node* new_node = new node(key);
        new_node->data = value;
        if (list_insert(&table[H::hash(key)], new_node))
            return true;

        delete new_node;
        return false;
    }

    bool hash_delete(K key)
    {
        return list_delete(&table[H::hash(key)], key);
    }

    bool hash_search(K key, T& result)
    {
        return list_search(&table[H::hash(key)], key, result);
    }

    // печать ключей в таблице
    void print_hash_table()
    {
        for (size_t i = 0; i < buckets; ++i)
        {
            marked_ptr curr = (*(table + i)).load();
            std::cout << i << " : ";
            while (curr != nullptr)
            {
                std::cout << curr->key.value << " ";
                curr = curr->next.load();
            }

            std::cout << std::endl;
        }
    }

    // получить сумму ключей в таблице
    int get_sum()
    {
        int sum = 0;
        for (size_t i = 0; i < buckets; ++i)
        {
            marked_ptr curr = (*(table + i)).load();

            while (curr != nullptr)
            {
                sum += curr->key.value;
                curr = curr->next.load();
            }
        }

        return sum;
    }

protected:
    std::atomic<marked_ptr>* prev;
    marked_ptr curr, next;

    marked_ptr list_find(std::atomic<marked_ptr>* head, K key,
                   std::atomic<marked_ptr>** out_prev, marked_ptr* out_next)
    {
        std::atomic<marked_ptr>* prev;
        marked_ptr curr, next;

        try_again:

        prev = head;
        curr = (*prev).load();
        next = nullptr;

        std::atomic<void*>& hp0 = get_hazard_pointer_for_current_thread(0);
        std::atomic<void*>& hp1 = get_hazard_pointer_for_current_thread(1);
        std::atomic<void*>& hp2 = get_hazard_pointer_for_current_thread(2);

        hp1.store(curr);

        while (true)
        {
            if (get_ptr(curr) == nullptr)
                goto done;

            next = get_ptr(curr)->next.load();
            hp0.store(next);

            K ckey = get_ptr(curr)->key;

            if ((*prev).load() != curr)
                goto try_again;

            if (!get_bit(next))
            {
                if (ckey >= key)
                    goto done;

                prev=&(get_ptr(curr)->next);
                hp2.store(curr);
            } else
            {
                marked_ptr cur = get_ptr(curr);
                if (prev->compare_exchange_strong(cur, get_ptr(next)))
                {
                    reclaim_later(curr);
                }
                else
                {
                    goto try_again;
                }
            }

            curr = next;
            hp1.store(next);
        }

        done:

        *out_prev = prev;
        *out_next = next;
        return curr;
    }

    bool list_insert(std::atomic<marked_ptr>* head, marked_ptr new_node)
    {
        std::atomic<void*>& hp0 = get_hazard_pointer_for_current_thread(0);
        std::atomic<void*>& hp1 = get_hazard_pointer_for_current_thread(1);
        std::atomic<void*>& hp2 = get_hazard_pointer_for_current_thread(2);

        K key = new_node->key;
        bool result = false;

        std::atomic<marked_ptr>* prev;
        marked_ptr curr, next;

        while (true)
        {
            curr = list_find(head, key, &prev, &next);

            if (get_ptr(curr) != nullptr)
            {
                if (get_ptr(curr)->key == key)
                {
                    result = false;
                    break;
                }
            }

            new_node->next.store(get_ptr(curr));
            marked_ptr cur = get_ptr(curr);
            if (prev->compare_exchange_strong(cur, get_ptr(new_node)))
            {
                result = true;
                break;
            }
        }

        hp0.store(nullptr);
        hp1.store(nullptr);
        hp2.store(nullptr);

        return result;
    }

    bool list_delete(std::atomic<marked_ptr>* head, K key)
    {
        bool result = false;
        std::atomic<void*>& hp0 = get_hazard_pointer_for_current_thread(0);
        std::atomic<void*>& hp1 = get_hazard_pointer_for_current_thread(1);
        std::atomic<void*>& hp2 = get_hazard_pointer_for_current_thread(2);

        std::atomic<marked_ptr>* prev;
        marked_ptr curr, next;

        while (true)
        {
            curr = list_find(head, key, &prev, &next);
            if ((get_ptr(curr) == nullptr) || get_ptr(curr)->key != key)
            {
                result = false;
                break;
            }

            marked_ptr n = get_ptr(next);
            if (!(get_ptr(curr)->next).compare_exchange_strong
                    (n, set_bit(get_ptr(next), 1)))
                continue;

            marked_ptr cur = get_ptr(curr);
            if (prev->compare_exchange_strong(cur, get_ptr(next)))
            {
                reclaim_later(curr);
            }
            else
            {
                list_find(head, key, &prev, &next);
            }

            result = true;
            break;
        }

        hp0.store(nullptr);
        hp1.store(nullptr);
        hp2.store(nullptr);

        return result;
    }

    bool list_search(std::atomic<marked_ptr>* head, K key, T& result)
    {
        std::atomic<marked_ptr>* prev;
        marked_ptr res, next;

        std::atomic<void*>& hp0 = get_hazard_pointer_for_current_thread(0);
        std::atomic<void*>& hp1 = get_hazard_pointer_for_current_thread(1);
        std::atomic<void*>& hp2 = get_hazard_pointer_for_current_thread(2);

        res = list_find(head, key, &prev, &next);

        if (get_ptr(res) && (get_ptr(res)->key == key))
        {
            result = get_ptr(res)->data;

            hp0.store(nullptr);
            hp1.store(nullptr);
            hp2.store(nullptr);

            return true;
        }

        return false;
    }
};

} // namespace lock_free

#endif // LOCK_FREE_HASH_TABLE_H
