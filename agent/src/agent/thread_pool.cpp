// thread_pool.cpp — ThreadPool implementation
#include "agent/thread_pool.hpp"

#include <iostream>

namespace agent {

// ---------------------------------------------------------------------------
// Constructor — spawn num_threads worker threads
// ---------------------------------------------------------------------------
ThreadPool::ThreadPool(std::size_t num_threads)
{
    m_workers.reserve(num_threads);
    for (std::size_t i = 0; i < num_threads; ++i) {
        m_workers.emplace_back([this] {
            for (;;) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(m_mutex);
                    m_cv.wait(lock, [this] {
                        return m_stop || !m_tasks.empty();
                    });

                    if (m_stop && m_tasks.empty()) {
                        return;  // clean exit
                    }

                    task = std::move(m_tasks.front());
                    m_tasks.pop();
                }
                try {
                    task();
                } catch (const std::exception& ex) {
                    std::cerr << "[ThreadPool] uncaught exception in worker: "
                              << ex.what() << "\n";
                } catch (...) {
                    std::cerr << "[ThreadPool] uncaught unknown exception in worker\n";
                }
            }
        });
    }
}

// ---------------------------------------------------------------------------
// Destructor — delegates to shutdown()
// ---------------------------------------------------------------------------
ThreadPool::~ThreadPool()
{
    shutdown();
}

// ---------------------------------------------------------------------------
// shutdown — signal stop and join all workers (idempotent)
// ---------------------------------------------------------------------------
void ThreadPool::shutdown()
{
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (m_stop) return;   // already stopped
        m_stop = true;
    }
    m_cv.notify_all();

    for (auto& w : m_workers) {
        if (w.joinable()) {
            w.join();
        }
    }
}

} // namespace agent
