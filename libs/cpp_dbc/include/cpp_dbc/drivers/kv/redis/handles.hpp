#pragma once

#include <memory>
#include <mutex>

// Forward declarations for hiredis
struct redisReply;
struct redisContext;

namespace cpp_dbc::Redis
{
    /**
     * @brief Alias for the shared connection mutex used by all driver classes.
     *
     * A `shared_ptr<recursive_mutex>` so that child objects (if any in the future)
     * can safely acquire the connection-level lock through a `weak_ptr<Connection>`.
     */
    using SharedConnMutex = std::shared_ptr<std::recursive_mutex>;
    /**
     * @brief Custom deleter for redisReply.
     *
     * RAII deleter that calls `freeReplyObject()` on the owned
     * `redisReply*`. Intended for use with `std::unique_ptr`.
     *
     * @see RedisReplyHandle
     */
    struct RedisReplyDeleter
    {
        void operator()(redisReply *reply);
    };

    /**
     * @brief Custom deleter for redisContext.
     *
     * RAII deleter that calls `redisFree()` on the owned
     * `redisContext*`. Intended for use with `std::shared_ptr`
     * or `std::unique_ptr`.
     */
    struct RedisContextDeleter
    {
        void operator()(redisContext *context);
    };

} // namespace cpp_dbc::Redis
