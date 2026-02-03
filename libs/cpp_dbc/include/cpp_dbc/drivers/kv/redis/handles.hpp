#pragma once

#include <memory>

// Forward declarations for hiredis
struct redisReply;
struct redisContext;

namespace cpp_dbc::Redis
{
        /**
         * @brief Custom deleter for redisReply
         */
        struct RedisReplyDeleter
        {
            void operator()(redisReply *reply);
        };

        /**
         * @brief Custom deleter for redisContext
         */
        struct RedisContextDeleter
        {
            void operator()(redisContext *context);
        };

} // namespace cpp_dbc::Redis
