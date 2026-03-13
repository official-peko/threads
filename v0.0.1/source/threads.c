#include <stdio.h>
#include <stdbool.h>
#include <stdatomic.h>

void *gc_alloc(int bytes);
int gc_track_current_stack();
void gc_remove_stack(int idx);

  // -------------------------------- //  
 // ---- MUTEXES IMPLEMENTATION ---- //
// -------------------------------- //

void atomic_mutex_flag_init(bool* flag) {
    atomic_flag default_flag = ATOMIC_FLAG_INIT;
    *((atomic_flag*)flag) = default_flag;
}

void atomic_mutex_flag_lock(bool* flag) {
    while (atomic_flag_test_and_set((atomic_flag*)flag)) {}
}

void atomic_mutex_flag_unlock(bool* flag) {
    atomic_flag_clear((atomic_flag*)flag);
}

  // -------------------------------- //  
 // ---- THREADS IMPLEMENTATION ---- //
// -------------------------------- //

typedef struct FunctionData
{
    void (*worker)(void *);
    void *data;
    int stack_idx;
} FunctionData;


#if defined(__unix__) || defined(__APPLE__) || defined(__linux__)
void *gc_thread_worker(void *data)
#else
unsigned long gc_thread_worker(void *data)
#endif
{
    int stack_idx = gc_track_current_stack();

    FunctionData *functiondata = data;
    functiondata->stack_idx = stack_idx;
    functiondata->worker(functiondata->data);

    gc_remove_stack(stack_idx);
    functiondata->stack_idx = -1;

#if defined(__unix__) || defined(__APPLE__) || defined(__linux__)
    return NULL;
#else
    return 0;
#endif
}

typedef struct Thread {
    void* thread;
    FunctionData* thread_function;
} Thread;

void clean_thread(void* thread) {
    Thread* thread_object = (Thread*)thread;
    if(thread_object->thread_function->stack_idx >= 0) {
        gc_remove_stack(thread_object->thread_function->stack_idx);
    }
}

#if defined(__unix__) || defined(__APPLE__) || defined(__linux__)
#include <pthread.h>

void *create_thread(void (*worker)(void *), void *data, bool synchronous)
{
    FunctionData *functiondata = (FunctionData *)gc_alloc(sizeof(FunctionData));
    functiondata->worker = worker;
    functiondata->data = data;
    functiondata->stack_idx = -1;

    pthread_t thread;
    pthread_create(&thread, NULL, gc_thread_worker, functiondata);

    Thread* thread_object = (Thread*)gc_alloc(sizeof(Thread));
    thread_object->thread = (void*)thread;
    thread_object->thread_function = functiondata;

    if (synchronous)
    {
        pthread_join(thread, NULL);
    }
    else
    {
        pthread_detach(thread);
    }

    return thread_object;
}

void terminate_thread(void *thread)
{
#ifdef __ANDROID__
    pthread_kill((pthread_t)((Thread*)thread)->thread, SIGUSR2);
#else
    pthread_cancel((pthread_t)((Thread*)thread)->thread);
#endif
    clean_thread(thread);
}
#endif

#ifdef _WIN32
#include <windows.h>

void *create_thread(void (*worker)(void *), void *data, bool synchronous)
{
    FunctionData *functiondata = (FunctionData *)gc_alloc(sizeof(FunctionData));
    functiondata->worker = worker;
    functiondata->data = data;

    DWORD id;
    void* thread = CreateThread(
        NULL,
        0,
        gc_thread_worker,
        functiondata,    
        0,         
        &id);

    Thread* thread_object = (Thread*)gc_alloc(sizeof(Thread));
    thread_object->thread = (void*)thread;
    thread_object->thread_function = functiondata;
    
    if(synchronous) {
        WaitForSingleObject(thread, INFINITE);
        CloseHandle(thread);
    }

    return thread_object;
}

void terminate_thread(void *thread)
{
    TerminateThread(((Thread*)thread)->thread, 0);
    CloseHandle(((Thread*)thread)->thread);
    clean_thread(thread);
}
#endif