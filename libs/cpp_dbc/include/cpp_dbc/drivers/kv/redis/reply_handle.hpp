#pragma once

#include "handles.hpp"
#include <memory>

// Forward declaration for hiredis
struct redisReply;

namespace cpp_dbc::Redis
{
    /**
     * @brief RAII wrapper for redisReply.
     *
     * Owns a `redisReply*` through a `std::unique_ptr` with
     * RedisReplyDeleter, ensuring the reply is freed when the
     * handle goes out of scope. Move-only (non-copyable).
     *
     * ### Example
     *
     * ```cpp
     * RedisReplyHandle reply = connection->executeRaw("GET", {"mykey"});
     * if (reply.get() != nullptr) {
     *     // use reply.get()->str, reply.get()->integer, etc.
     * }
     * ```
     */
    class RedisReplyHandle // NOSONAR - Rule of 5 satisfied: destructor is = default because unique_ptr handles resource
    {
    public:
        /**
         * @brief Nothrow constructor — takes ownership of a reply pointer without null-checking.
         *
         * Callers must validate the pointer before constructing. This variant is used in
         * noexcept contexts (e.g., executeRaw(std::nothrow_t)) where the reply has already
         * been validated as non-null and non-error before being handed off.
         */
        RedisReplyHandle(std::nothrow_t, redisReply *reply) noexcept;

        ~RedisReplyHandle();

        // Disable copy operations
        RedisReplyHandle(const RedisReplyHandle &) = delete;
        RedisReplyHandle &operator=(const RedisReplyHandle &) = delete;

        // Enable move operations
        RedisReplyHandle(RedisReplyHandle &&other) noexcept;
        RedisReplyHandle &operator=(RedisReplyHandle &&other) noexcept;

        redisReply *get() const;

    private:
        std::unique_ptr<redisReply, RedisReplyDeleter> m_reply;
    };

} // namespace cpp_dbc::Redis
