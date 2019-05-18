#ifndef LOCKED_HASH_TABLE_H
#define LOCKED_HASH_TABLE_H

#include "hash.h"

#include <iostream>
#include <functional>
#include <unordered_map>
#include <mutex>

template <typename K, typename T>
class lock_based_hash_table
{
public:
    lock_based_hash_table(size_t mb = max_buckets)
    {
        buckets = mb;
        size_t bucket_size = mb;

        data.max_load_factor(bucket_size);
        data.rehash(buckets);
    }

    bool hash_insert(K key, const T& value)
    {
        std::lock_guard<std::mutex> lock(m);
        data.insert({key, value});
        return true;
    }

    size_t hash_delete(K key)
    {
        std::lock_guard<std::mutex> lock(m);
        return data.erase(key);
    }

    void print_table()
    {
        for (size_t i = 0; i < buckets; ++i)
        {
            std::cout << i << " : ";
            for (auto it = data.begin(i); it!= data.end(i); ++it)
            {
               std::cout << it->first.value << " ";
            }

            std::cout << std::endl;
        }
    }

    int get_sum()
    {
        int sum = 0;
        for (size_t i = 0; i < buckets; ++i)
        {
            for (auto it = data.begin(i); it!= data.end(i); ++it)
            {
               sum += it->first.value;
            }
        }

        return sum;
    }

protected:
    size_t buckets;

    mutable std::mutex m;
    std::unordered_map<K, T> data;
};

#endif // LOCKED_HASH_TABLE_H
