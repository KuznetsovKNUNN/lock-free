#ifndef HAZARD_POINTER_H
#define HAZARD_POINTER_H

// based on Williams' C++ concurrency in action, ch. 7

#include <algorithm>
#include <atomic>
#include <functional>
#include <thread>
#include <vector>

namespace lock_free {

// максимальное количество hazard указателей
const unsigned int max_hazard_pointers   = 100;
// количество hazard указателей доступных каждому потоку
const unsigned int max_hp_per_thread     = 2;
// максимальный размер массива отложенных для удаления элементов
const unsigned int max_reclaim_list_size = 1000;

struct hazard_pointer
{
    std::atomic<std::thread::id> id;
    std::atomic<void*> pointer;
};

std::vector<hazard_pointer> hazard_pointers(max_hazard_pointers);

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

            // попытка завладеть hazard указателем
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

std::atomic<void*>& get_hazard_pointer_for_current_thread(size_t i)
{
    // у каждого потока свои hazard указатели
    thread_local static hp_owner hp[max_hp_per_thread];
    return hp[i].get_pointer();
}

// проверка указателя на присутствие в массиве hazard указателей
bool hazard(void* p)
{
    for (size_t i = 0; i < max_hazard_pointers; ++i)
    {
        if (hazard_pointers[i].pointer.load() == p)
            return true;
    }

    return false;
}

struct data_to_reclaim;

// уникальный для каждого потока массив отложенных для удаления элементов
thread_local static std::vector<data_to_reclaim*> reclaim_list;

template <typename T>
void do_delete(void* p)
{
    delete static_cast<T*>(p);
}

// структура, сохраняющая информацию о типе объекта
// для последующего корректного удаления
struct data_to_reclaim
{
    void* data;
    std::function<void(void*)> deleter;

    template <typename T>
    data_to_reclaim(T* p):
        data(p),
        deleter(&do_delete<T>) { }

    ~data_to_reclaim()
    {
        deleter(data);
    }
};

// освобождение безопасных указателей
void delete_nodes_with_no_hazards()
{
    std::vector<void*> hp;

    // добавляем все ненулевые hazard указатели в массив hp
    for (size_t i = 0; i < max_hazard_pointers; ++i)
    {
        void* p = hazard_pointers[i].pointer.load();
        if (p)
            hp.push_back(p);
    }

    // сортируем для удобного поиска
    sort(hp.begin(), hp.end(), std::less<void*>());

    auto i = reclaim_list.begin();
    while (i != reclaim_list.end())
    {
        // если указатель не в списке опасных, удаляем
        if (!std::binary_search(hp.begin(), hp.end(), (*i)->data))
        {
            delete *i;
            if (&*i != &reclaim_list.back())
                *i = reclaim_list.back();
            reclaim_list.pop_back();
        }
        else ++i;
    }
}

void add_to_reclaim_list(data_to_reclaim* data)
{
    reclaim_list.push_back(data);

    // при достижении макс. размера
    // пробуем удалить элементы, не отмеченные как hazard
    if (reclaim_list.size() == max_reclaim_list_size)
        delete_nodes_with_no_hazards();
}

template <typename T>
void reclaim_later(T* data)
{
    add_to_reclaim_list(new data_to_reclaim(data));
}

} // namespace lock_free

#endif // HAZARD_POINTER_H
