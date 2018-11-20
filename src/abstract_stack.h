#ifndef ABSTRACT_STACK_H
#define ABSTRACT_STACK_H

namespace lock_free {

template <typename T>
class stack
{
public:
    virtual bool push(const T& value) = 0;
    virtual bool pop(T& result) = 0;
};

} // namespace lock_free

#endif // ABSTRACT_STACK_H
