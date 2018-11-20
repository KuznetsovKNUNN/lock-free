#ifndef LOCK_BASED_QUEUE_H
#define LOCK_BASED_QUEUE_H

#include "abstract_queue.h"

#include <queue>
#include <mutex>

namespace lock_free {

// thread-safe queue using locks
template <typename T>
class lock_based_queue: public queue<T>
{
public:
    bool enqueue(const T& value) override
    {
        std::lock_guard<std::mutex> lock(m);
        data.push(value);
        return true;
    }

    bool dequeue(T& result) override
    {
        std::lock_guard<std::mutex> lock(m);
        if (data.empty())
            return false;
        result = std::move(data.front());
        data.pop();
        return true;
    }

    bool empty() const
    {
        std::lock_guard<std::mutex> lock(m);
        return data.empty();
    }

protected:
    mutable std::mutex m;
    std::queue<T> data;
};

} // namespace lock_free

#endif // LOCK_BASED_QUEUE_H
