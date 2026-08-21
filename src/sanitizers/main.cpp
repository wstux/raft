#include <iostream>
#include <limits>
#include <thread>
#include <vector>

int shared_counter = 0; // Common variable for threads

void increment_counter()
{
    for (int i = 0; i < 100000; ++i) {
        // Concurrent write to a shared variable without a mutex.
        shared_counter++;
    }
}

void trigger_buffer_overflow()
{
    std::cout << "Run address sanitizer test" << std::endl;

    std::vector<int> v = {1, 2, 3};
    int invalid_value = v[5]; // Out of bounds error
    std::cout << "Value (mast not be printed): " << invalid_value << std::endl;
}

void trigger_memory_leak()
{
    std::cout << "Run leak sanitizer test" << std::endl;

    // Allocate memory in the heap
    int* leaked_pointer = new int(42);
    std::cout << "Value: " << *leaked_pointer << std::endl;
    // Memory leak error
}

void trigger_thread()
{
    std::cout << "Run thread sanitizer test" << std::endl;

    std::thread t1(increment_counter);
    std::thread t2(increment_counter);

    t1.join();
    t2.join();

    std::cout << "Counter: " << shared_counter << std::endl;
}

void trigger_undefined_behavior()
{
    std::cout << "Run undefined behavior sanitizer test" << std::endl;

    // Integer overflow error
    int max_int = std::numeric_limits<int>::max();
    int overflowed = max_int + 1; // Undefined behavior
    std::cout << "Overflowed value: " << overflowed << std::endl;

    // 2. Divide by zero
    volatile int zero = 0;
    int bad_division = 42 / zero;
    std::cout << "Division result: " << bad_division << std::endl;
}

int main()
{
    trigger_buffer_overflow();
    trigger_memory_leak();
    trigger_thread();
    trigger_undefined_behavior();
    return 0;
}
