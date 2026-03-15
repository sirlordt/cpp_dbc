#ifndef CPP_DBC_COMMON_SERIAL_QUEUE_HPP
#define CPP_DBC_COMMON_SERIAL_QUEUE_HPP

#include <algorithm>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>

#include "cpp_dbc/common/system_utils.hpp"

// Debug output controlled by -DDEBUG_SERIAL_QUEUE=1 or -DDEBUG_ALL=1 CMake option
#if (defined(DEBUG_SERIAL_QUEUE) && DEBUG_SERIAL_QUEUE) || (defined(DEBUG_ALL) && DEBUG_ALL)
#define SQ_DEBUG(format, ...)                                                                    \
    do                                                                                           \
    {                                                                                            \
        char sq_debug_buf[1024];                                                                 \
        int sq_debug_n = std::snprintf(sq_debug_buf, sizeof(sq_debug_buf), format, ##__VA_ARGS__); \
        if (sq_debug_n >= static_cast<int>(sizeof(sq_debug_buf)))                                \
        {                                                                                        \
            static constexpr const char sq_trunc[] = "...[TRUNCATED]";                           \
            std::memcpy(sq_debug_buf + sizeof(sq_debug_buf) - sizeof(sq_trunc),                  \
                        sq_trunc, sizeof(sq_trunc));                                              \
        }                                                                                        \
        cpp_dbc::system_utils::logWithTimesMillis("SerialQueue", sq_debug_buf);                  \
    } while (0)
#else
#define SQ_DEBUG(format, ...) ((void)0)
#endif

namespace cpp_dbc
{

    /// Single-threaded async task queue. Tasks are executed FIFO by a dedicated
    /// worker thread. Designed for lightweight deferred work (e.g. registry cleanup)
    /// that should not block the caller.
    ///
    /// Each instance has a name used in debug output. Use global() to access
    /// named shared instances ("default" if no name is specified).
    class SerialQueue
    {
        // ── private (class default) ──────────────────────────────────────────
        // IMPORTANT: m_worker MUST be declared LAST. Members are destroyed in
        // reverse declaration order. ~m_worker calls join(), which waits for the
        // worker thread to finish. The worker thread uses m_mutex and m_cv, so
        // they must still be alive when join() runs.
        std::string m_name;
        std::queue<std::function<void()>> m_queue;
        mutable std::mutex m_mutex;
        std::condition_variable m_cv;
        std::jthread m_worker;

        void run(std::stop_token st) noexcept
        {
            SQ_DEBUG("[%s] Worker thread started", m_name.c_str());
            while (true)
            {
                std::function<void()> task;
                {
                    std::unique_lock lock(m_mutex);
                    m_cv.wait(lock, [this, &st]
                              { return !m_queue.empty() || st.stop_requested(); });

                    if (st.stop_requested() && m_queue.empty())
                    {
                        SQ_DEBUG("[%s] Stop requested, queue empty — exiting", m_name.c_str());
                        break;
                    }

                    task = std::move(m_queue.front());
                    m_queue.pop();
                }
                if (task)
                {
                    SQ_DEBUG("[%s] Executing task (pending: %zu)", m_name.c_str(), pending());
                    try
                    {
                        task();
                    }
                    catch ([[maybe_unused]] const std::exception &ex)
                    {
                        // Intentionally swallowed — tasks are fire-and-forget cleanup
                        // lambdas; letting an exception escape would call std::terminate
                        // because run() is noexcept.
                        SQ_DEBUG("[%s] Task threw std::exception: %s", m_name.c_str(), ex.what());
                    }
                    catch (...) // NOSONAR(cpp:S2738) — fallback for non-std exceptions after typed catch above
                    {
                        // Intentionally swallowed — same rationale as above.
                        SQ_DEBUG("[%s] Task threw unknown exception", m_name.c_str());
                    }
                }
            }
            SQ_DEBUG("[%s] Worker thread exiting", m_name.c_str());
        }

    public:
        explicit SerialQueue(std::string name = "default")
            : m_name(system_utils::toLowerCase(name)),
              m_worker([this](std::stop_token st)
                       { run(st); })
        {
            // Intentionally empty — worker started via m_worker initializer
        }

        ~SerialQueue()
        {
            SQ_DEBUG("[%s] Destructor — requesting stop (pending: %zu)", m_name.c_str(), pending());
            m_worker.request_stop();
            m_cv.notify_one();
            // std::jthread joins automatically in its destructor
        }

        SerialQueue(const SerialQueue &) = delete;
        SerialQueue &operator=(const SerialQueue &) = delete;

        /// Queue name (for debug/diagnostic purposes).
        [[nodiscard]] const std::string &name() const noexcept { return m_name; }

        /// Post a fire-and-forget task. Non-blocking.
        void post(std::function<void()> task)
        {
            {
                std::scoped_lock lock(m_mutex);
                m_queue.push(std::move(task));
                SQ_DEBUG("[%s] Task posted (pending: %zu)", m_name.c_str(), m_queue.size());
            }
            m_cv.notify_one();
        }

        /// Number of tasks waiting in the queue (not including the currently executing one).
        [[nodiscard]] size_t pending() const
        {
            std::scoped_lock lock(m_mutex);
            return m_queue.size();
        }

        /// Get a named shared instance. Names are case-insensitive ("MI_COLA" == "mi_cola").
        /// "default" if no name is specified.
        /// Each unique name creates a separate SerialQueue with its own worker thread.
        static SerialQueue &global(const std::string &name = "default") // NOSONAR(cpp:S6018) — function-local statics for thread-safe lazy init; inline variable would cause static initialization order fiasco
        {
            static std::mutex s_registryMutex;
            static std::unordered_map<std::string, std::unique_ptr<SerialQueue>> s_registry;

            std::string key = system_utils::toLowerCaseByCopy(name);
            std::scoped_lock lock(s_registryMutex);
            auto it = s_registry.find(key);
            if (it != s_registry.end())
            {
                SQ_DEBUG("[%s] Returning existing instance", key.c_str());
                return *it->second;
            }
            SQ_DEBUG("[%s] Creating new instance", key.c_str());
            auto [newIt, inserted] = s_registry.emplace(key, std::make_unique<SerialQueue>(key));
            return *newIt->second;
        }
    };

} // namespace cpp_dbc

#endif // CPP_DBC_COMMON_SERIAL_QUEUE_HPP
