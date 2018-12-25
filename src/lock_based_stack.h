#ifndef LOCK_BASED_STACK_H
#define LOCK_BASED_STACK_H

#include "abstract_stack.h"

#include <list>
#include <mutex>
#include <stack>

namespace lock_free {

// thread-safe стек с блокировкой (std::mutex)
template <typename T>
class lock_based_stack: public stack<T>
{
public:
    bool push(const T& value) override
    {
        std::lock_guard<std::mutex> lock(m);
        data.push(value);
        return true;
    }

    bool pop(T& result) override
    {
        std::lock_guard<std::mutex> lock(m);
        if (data.empty())
            return false;
        result = data.top();
        data.pop();
        return true;
    }

    bool empty() const
    {
        std::lock_guard<std::mutex> lock(m);
        return data.empty();
    }

protected:
    std::stack<T, std::list<T>> data;
    mutable std::mutex m;
};

} // namespace lock_free

#endif // LOCK_BASED_STACK_H
