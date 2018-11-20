#ifndef ABSTRACT_QUEUE_H
#define ABSTRACT_QUEUE_H

namespace lock_free {

template <typename T>
class queue
{
public:
    virtual bool enqueue(const T& value) = 0;
    virtual bool dequeue(T& result) = 0;
};

} // namespace lock_free

#endif // ABSTRACT_QUEUE_H
