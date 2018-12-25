#include "tagged_lock_free_stack.h"
#include "lock_based_stack.h"
#include "hazard_lock_free_stack.h"

#include "hazard_lock_free_queue.h"
#include "lock_based_queue.h"
#include "tagged_lock_free_queue.h"

// boost lock-free containers
#include <boost/lockfree/stack.hpp>
#include <boost/lockfree/queue.hpp>

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
const int num_elements   = 100;
const int num_threads    = 4;
const int num_operations = 10000;

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

            std::srand(unsigned(time(0)));

            // каждый поток достает из случайного контейнера элемент,
            // потом кладет этот же элемент в случайный контейнер
            // всего num_operations таких операций
            for (int j = 0; j < num_operations; ++j)
            {
                T val;
                if (((containers[rand() & 1].operator ->())->*get)(val))
                    ((containers[rand() & 1].operator ->())->*put)(val);
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

template <typename T>
bool boost_stack_test(int num_elements, int num_threads, int num_operations)
{
    std::cout << "==============================="  << std::endl;
    std::cout << "testing boost lock-free stack: "  << std::endl;

    std::unique_ptr<boost::lockfree::stack<T>> c1(new boost::lockfree::stack<T>(num_elements));
    std::unique_ptr<boost::lockfree::stack<T>> c2(new boost::lockfree::stack<T>(num_elements));
    std::vector<std::unique_ptr<boost::lockfree::stack<T>>> containers;
    containers.emplace_back(std::move(c1));
    containers.emplace_back(std::move(c2));

    T sum1 = T();
    for (int i = 0; i < num_elements; ++i)
    {
        T val = static_cast<T>(i);
        sum1 += val;
        (containers[i % 2].operator ->())->push(val);
    }

    std::vector< std::future<void> > futs;
    std::atomic<int> threads(num_threads);

    auto start_time = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < num_threads; ++i)
        futs.push_back(std::async(std::launch::async, [&, i]()
        {
            --threads;
            while (threads.load())
                std::this_thread::sleep_for(std::chrono::milliseconds(5));

            std::srand(unsigned(time(0)));

            for (int j = 0; j < num_operations; ++j)
            {
                T val;
                if ((containers[rand() & 1].operator ->())->pop(val))
                    (containers[rand() & 1].operator ->())->push(val);
            }
        }));

    for (auto& fut : futs)
        fut.get();

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> dur =
            std::chrono::duration_cast<
            std::chrono::duration<double>>(end_time - start_time);
    auto time = dur.count();

    T sum2 = T();
    int node_count = 0;
    for (int i = 0; i < 2; ++i)
    {
        T val;
        while ((containers[i].operator ->())->pop(val))
        {
            node_count++;
            sum2 += val;
        }
    }

    bool correct = (node_count == num_elements) && (sum1 == sum2);
    if (correct) std::cout << "correct, ";
    else std::cout << "error, ";

    std::cout << "work time: " << (time * 1000) << "ms" << std::endl;

    return correct;
}

template <typename T>
bool boost_queue_test(int num_elements, int num_threads, int num_operations)
{
    std::cout << "==============================="  << std::endl;
    std::cout << "testing boost lock-free queue: "  << std::endl;

    std::unique_ptr<boost::lockfree::queue<T>> c1(new boost::lockfree::queue<T>(num_elements));
    std::unique_ptr<boost::lockfree::queue<T>> c2(new boost::lockfree::queue<T>(num_elements));
    std::vector<std::unique_ptr<boost::lockfree::queue<T>>> containers;
    containers.emplace_back(std::move(c1));
    containers.emplace_back(std::move(c2));

    T sum1 = T();
    for (int i = 0; i < num_elements; ++i)
    {
        T val = static_cast<T>(i);
        sum1 += val;
        (containers[i % 2].operator ->())->push(val);
    }

    std::vector< std::future<void> > futs;
    std::atomic<int> threads(num_threads);

    auto start_time = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < num_threads; ++i)
        futs.push_back(std::async(std::launch::async, [&, i]()
        {
            --threads;
            while (threads.load())
                std::this_thread::sleep_for(std::chrono::milliseconds(5));

            std::srand(unsigned(time(0)));

            for (int j = 0; j < num_operations; ++j)
            {
                T val;
                if ((containers[rand() & 1].operator ->())->pop(val))
                    (containers[rand() & 1].operator ->())->push(val);
            }
        }));

    for (auto& fut : futs)
        fut.get();

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> dur =
            std::chrono::duration_cast<
            std::chrono::duration<double>>(end_time - start_time);
    auto time = dur.count();

    T sum2 = T();
    int node_count = 0;
    for (int i = 0; i < 2; ++i)
    {
        T val;
        while ((containers[i].operator ->())->pop(val))
        {
            node_count++;
            sum2 += val;
        }
    }

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
    char data[100000];
};

template <typename T>
void run_tests()
{
    std::cout << num_threads << " threads working..." << std::endl;
    run_stack_tests<T>();
    boost_stack_test<T>(num_elements, num_threads, num_operations);
    run_queue_tests<T>();
    boost_queue_test<T>(num_elements, num_threads, num_operations);
}

int main()
{
    run_tests<test_struct>();

    return 0;
}
