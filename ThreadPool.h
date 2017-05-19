#ifndef THREADPOOL_H_INCLUDED
#define THREADPOOL_H_INCLUDED

#include <vector>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <future>
#include <functional>

class ThreadPool {
public:
    ThreadPool() = default;
    ~ThreadPool();
    void initialize(size_t);
    template<class F, class... Args>
    auto add_task(F&& f, Args&&... args)
        -> std::future<typename std::result_of<F(Args...)>::type>;
private:
    std::vector<std::thread> m_threads;
    std::queue<std::function<void()>> m_tasks;

    std::mutex m_mutex;
    std::condition_variable m_condvar;
    bool m_exit{false};
};

inline void ThreadPool::initialize(size_t threads) {
    for (size_t i = 0; i < threads; i++) {
        m_threads.emplace_back([this] {
            for (;;) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(m_mutex);
                    m_condvar.wait(lock, [this]{ return m_exit || !m_tasks.empty(); });
                    if (m_exit && m_tasks.empty()) {
                        return;
                    }
                    task = std::move(m_tasks.front());
                    m_tasks.pop();
                }
                task();
            }
        });
    }
}

template<class F, class... Args>
auto ThreadPool::add_task(F&& f, Args&&... args)
    -> std::future<typename std::result_of<F(Args...)>::type> {
    using return_type = typename std::result_of<F(Args...)>::type;

    auto task = std::make_shared< std::packaged_task<return_type()> >(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );

    std::future<return_type> res = task->get_future();
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_tasks.emplace([task](){(*task)();});
    }
    m_condvar.notify_one();
    return res;
}

inline ThreadPool::~ThreadPool() {
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_exit = true;
    }
    m_condvar.notify_all();
    for (std::thread & worker: m_threads) {
        worker.join();
    }
}

class ThreadGroup {
public:
    ThreadGroup(ThreadPool & pool) : m_pool(pool) {};
    template<class F, class... Args>
    void add_task(F&& f, Args&&... args);
    void wait_all();
private:
    ThreadPool & m_pool;
    std::vector<std::future<void>> m_taskresults;
};

template<class F, class... Args>
inline void ThreadGroup::add_task(F&& f, Args&&... args) {
    m_taskresults.emplace_back(
        m_pool.add_task(std::forward<F>(f), std::forward<Args>(args)...)
    );
}

inline void ThreadGroup::wait_all() {
    for (auto && result: m_taskresults) {
        result.get();
    }
}

#endif
