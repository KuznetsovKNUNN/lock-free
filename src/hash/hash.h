#ifndef HASH_H
#define HASH_H

#include "tbb/concurrent_hash_map.h"

const size_t max_buckets = 256;

struct key
{
    int value;

    key(int key): value(key) { }

    bool operator == (const key& other) const
    {
        return (value == other.value);
    }

    bool operator >= (const key& other) const
    {
        return (value >= other.value);
    }

    bool operator != (const key& other) const
    {
        return (value != other.value);
    }
};

// для locked hash table
namespace std
{
    template <>
    struct hash<key>
    {
        std::size_t operator()(const key& k) const
        {
            return (k.value) % max_buckets;
        }
    };
}

// для tbb и lock free hash table
struct my_hash
{
    static size_t hash(const key& key) {
        return (key.value) % max_buckets;
    }

    static bool equal(const key& x, const key& y) {
        return x.value == y.value;
    }
};

#endif // HASH_H
