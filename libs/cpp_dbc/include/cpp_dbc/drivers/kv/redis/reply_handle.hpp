#pragma once

#include "handles.hpp"
#include <memory>

// Forward declaration for hiredis
struct redisReply;

namespace cpp_dbc::Redis
{
        /**
         * @brief RAII wrapper for redisReply
         *
         * This class ensures that redisReply is freed when it goes out of scope.
         */
        class RedisReplyHandle // NOSONAR - Rule of 5 satisfied: destructor is = default because unique_ptr handles resource
        {
        public:
            explicit RedisReplyHandle(redisReply *reply);
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
