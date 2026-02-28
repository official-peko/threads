#include <thread>

extern "C" {
    void create_thread(void(*worker)(void*), char* data) {
        std::thread t1(worker, data);
        t1.detach();
    }
}