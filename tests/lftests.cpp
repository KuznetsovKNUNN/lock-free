#include "tagged_lock_free_stack.h"
#include "lock_based_stack.h"
#include "hazard_lock_free_stack.h"

#include "hazard_lock_free_queue.h"
#include "lock_based_queue.h"
#include "tagged_lock_free_queue.h"

#include "lock_free_hash_table.h"
#include "locked_hash_table.h"
#include "tbb/concurrent_hash_map.h"
#include "hash.h"

#include <atomic>
#include <chrono>
#include <future>
#include <iostream>
#include <memory>
#include <random>
#include <thread>
#include <vector>

using namespace lock_free;

// параметры теста:
const int num_elements   = 256;
const int num_threads    = 4;
const int num_operations = 10000;

int extra_work()
{
    int max_cycles = 1000;

    int sum = 0;
    for (int i = 0; i < max_cycles; ++i)
    {
        sum += i;
    }

    return sum;
}

template <typename T>
void lfht_test()
{
    std::cout << "=========" << std::endl;
    std::cout << "lock-free" << std::endl;

    lock_free_hash_table<key, T, my_hash> lfht;

    int sum1 = 0;
    for (int i = 0; i < num_elements; ++i)
    {
        sum1 += i;
        T data = T();
        key k(i);
        lfht.hash_insert(k, data);
    }

    std::vector< std::future<void> > futs;
    std::atomic<int> threads(num_threads);

    auto start_time = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < num_threads; ++i)
        futs.push_back(std::async(std::launch::async, [&]()
        {
            // ждем старта всех потоков
            --threads;
            while (threads.load())
                std::this_thread::sleep_for(std::chrono::milliseconds(5));

            std::srand(unsigned(time(nullptr)));

            for (int j = 0; j < num_operations; ++j)
            {
                int key = rand() % num_elements;
                if (lfht.hash_delete(key))
                {
                    extra_work();
                    T data = T();
                    lfht.hash_insert(key, data);
                }
            }
         }));

    for (auto& fut : futs)
        fut.get();

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> dur =
            std::chrono::duration_cast<
            std::chrono::duration<double>>(end_time - start_time);
    auto time = dur.count();

    int sum2 = lfht.get_sum();
    bool correct = (sum1 == sum2);
    if (correct) std::cout << "correct, ";
    else std::cout << "error, ";

    std::cout << "work time: " << (time * 1000) << "ms" << std::endl;
}

template <typename T>
void locked_test()
{
    std::cout << "==========" << std::endl;
    std::cout << "lock-based" << std::endl;

    lock_based_hash_table<key, T> locked_ht;

    int sum1 = 0;
    for (int i = 0; i < num_elements; ++i)
    {
        sum1 += i;
        T data = T();
        key k(i);
        locked_ht.hash_insert(k, data);
    }

    std::vector< std::future<void> > futs;
    std::atomic<int> threads(num_threads);

    auto start_time = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < num_threads; ++i)
        futs.push_back(std::async(std::launch::async, [&]()
        {
            // ждем старта всех потоков
            --threads;
            while (threads.load())
                std::this_thread::sleep_for(std::chrono::milliseconds(5));

            std::srand(unsigned(time(nullptr)));

            for (int j = 0; j < num_operations; ++j)
            {
                int key = rand() % num_elements;
                if (locked_ht.hash_delete(key))
                {
                    extra_work();
                    T data = T();
                    locked_ht.hash_insert(key, data);
                }
            }
         }));

    for (auto& fut : futs)
        fut.get();

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> dur =
            std::chrono::duration_cast<
            std::chrono::duration<double>>(end_time - start_time);
    auto time = dur.count();

    int sum2 = locked_ht.get_sum();
    bool correct = (sum1 == sum2);
    if (correct) std::cout << "correct, ";
    else std::cout << "error, ";

    std::cout << "work time: " << (time * 1000) << "ms" << std::endl;
}

template <typename T>
void tbb_test()
{
    std::cout << "==========" << std::endl;
    std::cout << "tbb" << std::endl;

    using table = tbb::concurrent_hash_map<key, T, my_hash>;
    table ht;

    int sum1 = 0;
    for (int i = 0; i < num_elements; ++i)
    {
        sum1 += i;
        T data = T();
        typename table::accessor a;
        ht.insert(a, key(i));
        a->second = data;
        a.release();
    }

    std::vector< std::future<void> > futs;
    std::atomic<int> threads(num_threads);

    auto start_time = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < num_threads; ++i)
        futs.push_back(std::async(std::launch::async, [&]()
        {
            // ждем старта всех потоков
            --threads;
            while (threads.load())
                std::this_thread::sleep_for(std::chrono::milliseconds(5));

            std::srand(unsigned(time(nullptr)));

            for (int j = 0; j < num_operations; ++j)
            {
                int val = rand() % num_elements;
                if (ht.erase(key(val)))
                {
                    extra_work();
                    T data = T();
                    typename table::accessor a;
                    ht.insert(a, key(val));
                    a->second = data;
                    a.release();
                }
            }
         }));

    for (auto& fut : futs)
        fut.get();

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> dur =
            std::chrono::duration_cast<
            std::chrono::duration<double>>(end_time - start_time);
    auto time = dur.count();

    int sum2 = 0;
    for (auto it = ht.begin(); it != ht.end(); ++it)
    {
        sum2 += it->first.value;
    }

    bool correct = (sum1 == sum2);
    if (correct) std::cout << "correct, ";
    else std::cout << "error, ";

    std::cout << "work time: " << (time * 1000) << "ms" << std::endl;
}

template <template <class> class Container, typename T,
          typename Put, typename Get>
bool container_test(std::vector<std::unique_ptr<Container<T>>> &containers,
                    Put put, Get get,
                    int num_elements,
                    int num_threads,
                    int num_operations)
{
    // добавляем num_elements элементов в контейнеры,
    // подсчитываем сумму элементов
    T sum1 = T();
    for (int i = 0; i < num_elements; ++i)
    {
        T val = static_cast<T>(i);
        sum1 += val;
        ((containers[i % 2].operator ->())->*put)(val);
    }

    std::vector< std::future<void> > futs;
    std::atomic<int> threads(num_threads);

    auto start_time = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < num_threads; ++i)
        futs.push_back(std::async(std::launch::async, [&, i]()
        {
            // ждем старта всех потоков
            --threads;
            while (threads.load())
                std::this_thread::sleep_for(std::chrono::milliseconds(5));

            std::srand(unsigned(time(nullptr)));

            // каждый поток достает из случайного контейнера элемент,
            // потом кладет этот же элемент в случайный контейнер
            // всего num_operations таких операций
            for (int j = 0; j < num_operations; ++j)
            {
                T val;
                if (((containers[rand() & 1].operator ->())->*get)(val))
                {
                    extra_work();
                    ((containers[rand() & 1].operator ->())->*put)(val);
                }
            }
        }));

    for (auto& fut : futs)
        fut.get();

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> dur =
            std::chrono::duration_cast<
            std::chrono::duration<double>>(end_time - start_time);
    auto time = dur.count();

    // подсчитываем итоговую сумму и количество элементов
    // после всех операций с контейнерами
    T sum2 = T();
    int node_count = 0;
    for (int i = 0; i < 2; ++i)
    {
        T val;
        while (((containers[i].operator ->())->*get)(val))
        {
            node_count++;
            sum2 += val;
        }
    }

    // проверка, что сумма и количество элементов в контейнерах не изменились
    bool correct = (node_count == num_elements) && (sum1 == sum2);
    if (correct) std::cout << "correct, ";
    else std::cout << "error, ";

    std::cout << "work time: " << (time * 1000) << "ms" << std::endl;

    return correct;
}

// создание lock-free контейнеров с использованием
// меченых указателей,
// присутствует второй шаблонный параметр - размер контейнера
template <template <class> class ContainerBase,
          template <class, size_t> class ContainerDerived, typename T, size_t N>
std::vector<std::unique_ptr<ContainerBase<T>>> create_tagged_containers()
{
    std::unique_ptr<ContainerDerived<T, N>> c1(new ContainerDerived<T, N>);
    std::unique_ptr<ContainerDerived<T, N>> c2(new ContainerDerived<T, N>);
    std::vector<std::unique_ptr<ContainerBase<T>>> containers;
    containers.emplace_back(std::move(c1));
    containers.emplace_back(std::move(c2));
    return containers;
}

// создание контейнеров,
// отсутствует второй шаблонный параметр у
// производного класса (размер контейнера)
template <template <class> class ContainerBase,
          template <class> class ContainerDerived, typename T>
std::vector<std::unique_ptr<ContainerBase<T>>> create_containers()
{
    std::unique_ptr<ContainerDerived<T>> c1(new ContainerDerived<T>);
    std::unique_ptr<ContainerDerived<T>> c2(new ContainerDerived<T>);
    std::vector<std::unique_ptr<ContainerBase<T>>> containers;
    containers.emplace_back(std::move(c1));
    containers.emplace_back(std::move(c2));
    return containers;
}

template <typename T>
void run_stack_tests()
{
    std::cout << "==============================="  << std::endl;
    std::cout << "testing lock-based stack:      "  << std::endl;

    auto lock_based_stacks = create_containers<stack, lock_based_stack, T>();
    container_test(lock_based_stacks, &stack<T>::push,
                   &stack<T>::pop, num_elements,
                   num_threads, num_operations);

    std::cout << "==============================="  << std::endl;
    std::cout << "testing tagged lock-free stack:"  << std::endl;

    auto tagged_lock_free_stacks = create_tagged_containers<stack,
            tagged_lock_free_stack, T, num_elements>();
    container_test(tagged_lock_free_stacks, &stack<T>::push,
                   &stack<T>::pop, num_elements,
                   num_threads, num_operations);

    std::cout << "==============================="  << std::endl;
    std::cout << "testing hazard lock-free stack:"  << std::endl;

    auto hazard_lock_free_stacks = create_containers<stack,
            hazard_lock_free_stack, T>();
    container_test(hazard_lock_free_stacks, &stack<T>::push,
                   &stack<T>::pop, num_elements,
                   num_threads, num_operations);
}

template <typename T>
void run_queue_tests()
{
    std::cout << "==============================="  << std::endl;
    std::cout << "testing lock-based queue:      "  << std::endl;

    auto lock_based_queues = create_containers<queue, lock_based_queue, T>();
    container_test(lock_based_queues, &queue<T>::enqueue,
                   &queue<T>::dequeue, num_elements,
                   num_threads, num_operations);

    std::cout << "==============================="  << std::endl;
    std::cout << "testing tagged lock-free queue:"  << std::endl;

    auto tagged_lock_free_queues = create_tagged_containers<queue,
            tagged_lock_free_queue, T, num_elements*2>();
    container_test(tagged_lock_free_queues, &queue<T>::enqueue,
                   &queue<T>::dequeue, num_elements,
                   num_threads, num_operations);

    std::cout << "==============================="  << std::endl;
    std::cout << "testing hazard lock-free queue:"  << std::endl;

    auto hazard_lock_free_queues = create_containers<queue,
            hazard_lock_free_queue, T>();
    container_test(hazard_lock_free_queues, &queue<T>::enqueue,
                   &queue<T>::dequeue, num_elements,
                   num_threads, num_operations);
}

struct test_struct
{
public:
    test_struct(): sum(0) { }
    test_struct(int _sum): sum(_sum) { }

    test_struct& operator+=(const test_struct& rhs)
    {
        this->sum += rhs.sum;
        return *this;
    }

    bool operator==(const test_struct& rhs)
    {
        return this->sum == rhs.sum;
    }

    int  sum;
    char data[1000];
};

template <typename T>
void run_hash_tests()
{
    lfht_test<T>();
    locked_test<T>();
    tbb_test<T>();
}

template <typename T>
void run_tests()
{
    std::cout << num_threads << " threads working..." << std::endl;
    run_stack_tests<T>();
    run_queue_tests<T>();
    run_hash_tests<T>();
}

int main()
{
    run_tests<test_struct>();

    return 0;
}
