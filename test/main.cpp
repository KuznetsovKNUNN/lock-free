#include "tagged_lock_free_stack.h"
#include "lock_based_stack.h"
#include "hazard_lock_free_stack.h"

#include "tagged_lock_free_queue.h"
#include "lock_based_queue.h"

#include <atomic>
#include <chrono>
#include <future>
#include <iostream>
#include <memory>
#include <random>
#include <thread>
#include <vector>

using namespace lock_free;

template <template <class> class Container, typename T,
          typename Put, typename Get>
bool container_test(std::vector< std::unique_ptr< Container<T> > > &containers,
                    Put put, Get get,
                    int num_elements,
                    int num_threads,
                    int num_operations)
{
    T sum1 = T();
    for (int i = 0; i < num_elements; ++i)
    {
        T val = static_cast<T>(i);
        sum1 += val;
        (containers[i % 2].get()->*put)(val);
    }

    std::vector< std::future<void> > futs;
    std::atomic<int> threads(num_threads);

    std::cout << num_threads << " threads working..." << std::endl;
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
                if ((containers[rand() & 1].get()->*get)(val))
                    (containers[rand() & 1].get()->*put)(val);
            }
        }));

    for (auto& fut : futs)
        fut.get();

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> dur =
            std::chrono::duration_cast<
            std::chrono::duration<double> >(end_time - start_time);
    auto time = dur.count();

    T sum2 = T();
    int node_count = 0;
    for (int i = 0; i < 2; ++i)
    {
        T val;
        while ((containers[i].get()->*get)(val))
        {
            node_count++;
            sum2 += val;
        }
    }

    std::cout << "number of nodes "  << node_count
              << "/" << num_elements << std::endl;
    std::cout << "sum " << sum2 << "/" << sum1 << std::endl;

    bool correct = (node_count == num_elements) && (sum1 == sum2);
    if (correct) std::cout << "correct" << std::endl;
    else std::cout << "error" << std::endl;

    std::cout << "work time: " << (time * 1000) << "ms" << std::endl;

    return correct;
}

// создание lock-free контейнеров с использованием
// отмеченных указателей,
// присутствует второй шаблонный параметр - размер контейнера
template <template <class> class ContainerBase,
          template <class, size_t> class ContainerDerived, typename T, size_t N>
std::vector< std::unique_ptr< ContainerBase<T> > > create_tagged_containers()
{
    std::unique_ptr< ContainerDerived<T, N> > c1(new ContainerDerived<T, N>);
    std::unique_ptr< ContainerDerived<T, N> > c2(new ContainerDerived<T, N>);
    std::vector< std::unique_ptr< ContainerBase<T> > > containers;
    containers.emplace_back(std::move(c1));
    containers.emplace_back(std::move(c2));
    return containers;
}

// создание контейнеров,
// отсутствует второй шаблонный параметр у
// производного класса (размер контейнера)
template <template <class> class ContainerBase,
          template <class> class ContainerDerived, typename T>
std::vector< std::unique_ptr< ContainerBase<T> > > create_containers()
{
    std::unique_ptr< ContainerDerived<T> > c1(new ContainerDerived<T>);
    std::unique_ptr< ContainerDerived<T> > c2(new ContainerDerived<T>);
    std::vector< std::unique_ptr< ContainerBase<T> > > containers;
    containers.emplace_back(std::move(c1));
    containers.emplace_back(std::move(c2));
    return containers;
}

template <typename T>
void run_stack_tests()
{
    // параметры теста:
    const int num_elements   = 100;     // число элементов в контейнере
    const int num_threads    = 4;       // число потоков
    const int num_operations = 100000;  // число операций каждого из потоков
                                        // с контейнером

    std::cout << "==============================="  << std::endl;
    std::cout << "testing tagged lock-free stack:"  << std::endl;
    std::cout << "==============================="  << std::endl;

    auto tagged_lock_free_stacks = create_tagged_containers<stack,
            tagged_lock_free_stack, T, num_elements>();

    container_test(tagged_lock_free_stacks, &stack<T>::push,
                   &stack<T>::pop, num_elements,
                   num_threads, num_operations);

    std::cout << "========================="  << std::endl;
    std::cout << "testing lock-based stack:"  << std::endl;
    std::cout << "========================="  << std::endl;

    auto lock_based_stacks = create_containers<stack,
            lock_based_stack, T>();

    container_test(lock_based_stacks, &stack<T>::push,
                   &stack<T>::pop, num_elements,
                   num_threads, num_operations);

    std::cout << "==============================="  << std::endl;
    std::cout << "testing hazard lock-free stack:"  << std::endl;
    std::cout << "==============================="  << std::endl;

    auto hazard_lock_free_stacks = create_containers<stack,
            hazard_lock_free_stack, T>();

    container_test(hazard_lock_free_stacks, &stack<T>::push,
                   &stack<T>::pop, num_elements,
                   num_threads, num_operations);
}

template <typename T>
void run_queue_tests()
{
    // параметры теста:
    const int num_elements   = 100;     // число элементов в контейнере
    const int num_threads    = 4;       // число потоков
    const int num_operations = 100000;  // число операций каждого из потоков
                                        // с контейнером

    std::cout << "==============================="  << std::endl;
    std::cout << "testing tagged lock-free queue:"  << std::endl;
    std::cout << "==============================="  << std::endl;

    auto tagged_lock_free_queues = create_tagged_containers<queue,
            tagged_lock_free_queue, T, num_elements*2>();

    container_test(tagged_lock_free_queues, &queue<T>::enqueue,
                   &queue<T>::dequeue, num_elements,
                   num_threads, num_operations);

    std::cout << "==============================="  << std::endl;
    std::cout << "testing lock-based queue:      "  << std::endl;
    std::cout << "==============================="  << std::endl;

    auto lock_based_queues = create_containers<queue,
            lock_based_queue, T>();

    container_test(lock_based_queues, &queue<T>::enqueue,
                   &queue<T>::dequeue, num_elements,
                   num_threads, num_operations);
}

template <typename T>
void run_tests()
{
    run_stack_tests<T>();

    run_queue_tests<T>();
}

int main()
{
    run_tests<int>();

    return 0;
}
