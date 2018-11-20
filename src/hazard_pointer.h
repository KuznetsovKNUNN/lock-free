#ifndef HAZARD_POINTER_H
#define HAZARD_POINTER_H

// based on Williams' C++ concurrency in action, ch. 7

#include <array>
#include <atomic>
#include <functional>
#include <thread>

namespace lock_free {

const unsigned int max_hazard_pointers = 10;

struct hazard_pointer
{
    std::atomic<std::thread::id> id;
    std::atomic<void*> pointer;
};

std::array<hazard_pointer, max_hazard_pointers> hazard_pointers;

class hp_owner
{
public:
    hp_owner(const hp_owner&) = delete;
    hp_owner operator=(const hp_owner&) = delete;

    hp_owner(): hp(nullptr)
    {
        for (size_t i = 0; i < max_hazard_pointers; ++i)
        {
            std::thread::id old_id;
            if (hazard_pointers[i].id.compare_exchange_strong(
                        old_id, std::this_thread::get_id()))
            {
                hp = &hazard_pointers[i];
                break;
            }
        }

        if (!hp)
            throw std::runtime_error("no hazard pointers available");
    }

    std::atomic<void*>& get_pointer()
    {
        return hp->pointer;
    }

    ~hp_owner()
    {
            hp->pointer.store(nullptr);
            hp->id.store(std::thread::id());
    }

protected:
    hazard_pointer *hp;
};

std::atomic<void*>& get_hazard_pointer_for_current_thread()
{
    thread_local static hp_owner hp;
    return hp.get_pointer();
}

bool hazard(void* p)
{
    for (size_t i = 0; i < max_hazard_pointers; ++i)
    {
        if (hazard_pointers[i].pointer.load() == p)
            return true;
    }

    return false;
}

template <typename T>
void do_delete(void* p)
{
    delete static_cast<T*>(p);
}

struct data_to_reclaim
{
    void* data;
    std::function<void(void*)> deleter;
    data_to_reclaim* next;

    template <typename T>
    data_to_reclaim(T* p):
        data(p),
        deleter(&do_delete<T>),
        next(0) { }

    ~data_to_reclaim()
    {
        deleter(data);
    }
};

std::atomic<data_to_reclaim*> nodes_to_reclaim;

void add_to_reclaim_list(data_to_reclaim* node)
{
    node->next = nodes_to_reclaim.load();
    while (!nodes_to_reclaim.compare_exchange_weak(node->next, node));
}

template <typename T>
void reclaim_later(T* data)
{
    add_to_reclaim_list(new data_to_reclaim(data));
}

void delete_nodes_with_no_hazards()
{
    data_to_reclaim *current = nodes_to_reclaim.exchange(nullptr);
    while (current)
    {
        data_to_reclaim* next = current->next;
        if (!hazard(current->data))
            delete current;
        else
            add_to_reclaim_list(current);
        current = next;
    }
}

} // namespace lock_free

#endif // HAZARD_POINTER_H
