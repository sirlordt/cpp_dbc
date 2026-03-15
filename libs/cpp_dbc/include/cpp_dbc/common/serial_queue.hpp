#ifndef CPP_DBC_COMMON_SERIAL_QUEUE_HPP
#define CPP_DBC_COMMON_SERIAL_QUEUE_HPP

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>

namespace cpp_dbc
{

    /// Single-threaded async task queue. Tasks are executed FIFO by a dedicated
    /// worker thread. Designed for lightweight deferred work (e.g. registry cleanup)
    /// that should not block the caller.
    class SerialQueue
    {
        // ── private (class default) ──────────────────────────────────────────
        // IMPORTANT: m_worker MUST be declared LAST. Members are destroyed in
        // reverse declaration order. ~m_worker calls join(), which waits for the
        // worker thread to finish. The worker thread uses m_mutex and m_cv, so
        // they must still be alive when join() runs.
        std::queue<std::function<void()>> m_queue;
        mutable std::mutex m_mutex;
        std::condition_variable m_cv;
        std::jthread m_worker;

        void run(std::stop_token st) noexcept
        {
            while (!st.stop_requested())
            {
                std::function<void()> task;
                {
                    std::unique_lock lock(m_mutex);
                    m_cv.wait(lock, [&]
                              { return !m_queue.empty() || st.stop_requested(); });

                    if (st.stop_requested() && m_queue.empty())
                    {
                        break;
                    }

                    task = std::move(m_queue.front());
                    m_queue.pop();
                }
                if (task)
                {
                    task();
                }
            }
        }

    public:
        SerialQueue()
            : m_worker([this](std::stop_token st)
                       { run(st); })
        {
            // Intentionally empty — worker started via m_worker initializer
        }

        ~SerialQueue()
        {
            m_worker.request_stop();
            m_cv.notify_one();
            // std::jthread joins automatically in its destructor
        }

        SerialQueue(const SerialQueue &) = delete;
        SerialQueue &operator=(const SerialQueue &) = delete;

        /// Post a fire-and-forget task. Non-blocking.
        void post(std::function<void()> task)
        {
            {
                std::scoped_lock lock(m_mutex);
                m_queue.push(std::move(task));
            }
            m_cv.notify_one();
        }

        /// Number of tasks waiting in the queue (not including the currently executing one).
        size_t pending() const
        {
            std::scoped_lock lock(m_mutex);
            return m_queue.size();
        }

        /// Global shared instance for lightweight deferred tasks.
        static SerialQueue &global()
        {
            static SerialQueue instance;
            return instance;
        }
    };

} // namespace cpp_dbc

#endif // CPP_DBC_COMMON_SERIAL_QUEUE_HPP
