#pragma once
#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <vector>

namespace agent {

class ThreadPool {
public:
    explicit ThreadPool(std::size_t num_threads);
    ~ThreadPool();

    ThreadPool(const ThreadPool&)            = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    template<typename F, typename... Args>
    auto submit(F&& f, Args&&... args)
        -> std::future<std::invoke_result_t<F, Args...>>
    {
        using R = std::invoke_result_t<F, Args...>;
        auto task = std::make_shared<std::packaged_task<R()>>(
            [func = std::forward<F>(f),
             tup  = std::make_tuple(std::forward<Args>(args)...)]() mutable -> R {
                return std::apply(std::move(func), std::move(tup));
            }
        );
        std::future<R> fut = task->get_future();
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            if (m_stop) throw std::runtime_error("ThreadPool is stopped");
            m_tasks.push([task]() { (*task)(); });
        }
        m_cv.notify_one();
        return fut;
    }

    void shutdown();
    std::size_t size() const { return m_workers.size(); }

private:
    std::vector<std::thread>            m_workers;
    std::queue<std::function<void()>>   m_tasks;
    std::mutex                          m_mutex;
    std::condition_variable             m_cv;
    bool                                m_stop{false};
};

} // namespace agent
