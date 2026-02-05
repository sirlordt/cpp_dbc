#pragma once

#include "../../../cpp_dbc.hpp"

#if USE_MYSQL
#include <mysql/mysql.h>
#include <memory>
#include <mutex>

namespace cpp_dbc::MySQL
{
        /**
         * @brief Custom deleter for MYSQL_RES* to use with unique_ptr
         *
         * This deleter ensures that mysql_free_result() is called automatically
         * when the unique_ptr goes out of scope, preventing memory leaks.
         */
        struct MySQLResDeleter
        {
            void operator()(MYSQL_RES *res) const noexcept
            {
                if (res)
                {
                    mysql_free_result(res);
                }
            }
        };

        /**
         * @brief Type alias for the smart pointer managing MYSQL_RES
         *
         * Uses unique_ptr with custom deleter to ensure automatic cleanup
         * of MySQL result sets, even in case of exceptions.
         */
        using MySQLResHandle = std::unique_ptr<MYSQL_RES, MySQLResDeleter>;

        /**
         * @brief Custom deleter for MYSQL_STMT* to use with unique_ptr
         *
         * Ensures mysql_stmt_close() is called automatically on destruction.
         */
        struct MySQLStmtDeleter
        {
            void operator()(MYSQL_STMT *stmt) const noexcept
            {
                if (stmt)
                {
                    mysql_stmt_close(stmt);
                }
            }
        };

        /** @brief RAII handle for MYSQL_STMT using unique_ptr with custom deleter */
        using MySQLStmtHandle = std::unique_ptr<MYSQL_STMT, MySQLStmtDeleter>;

        /**
         * @brief Type alias for shared connection mutex
         *
         * @details
         * This shared_ptr<recursive_mutex> is shared between a MySQLDBConnection and all
         * its PreparedStatements. This ensures that ALL operations that use the MYSQL*
         * connection (including mysql_stmt_close() in PreparedStatement destructors) are
         * serialized through the same mutex.
         *
         * **THE PROBLEM IT SOLVES:**
         *
         * Without a shared mutex, PreparedStatement uses its own mutex (m_mutex) while
         * Connection uses m_connMutex. When a PreparedStatement destructor runs (calling
         * mysql_stmt_close), it only locks its own mutex - NOT the connection's mutex.
         * This allows the destructor to run concurrently with connection operations in
         * other threads (like pool validation queries), causing use-after-free corruption.
         *
         * **HOW IT WORKS:**
         *
         * 1. Connection creates a shared mutex: m_connMutex = std::make_shared<std::recursive_mutex>()
         * 2. When creating a PreparedStatement, Connection passes its shared mutex
         * 3. PreparedStatement stores the same shared mutex
         * 4. ALL operations on both Connection and PreparedStatement lock the SAME mutex
         * 5. This includes PreparedStatement destructor calling mysql_stmt_close()
         * 6. Result: No race conditions possible
         */
        using SharedConnMutex = std::shared_ptr<std::recursive_mutex>;

        /**
         * @brief Custom deleter for MYSQL* to use with shared_ptr
         *
         * Ensures mysql_close() is called automatically on destruction.
         */
        struct MySQLDeleter
        {
            void operator()(MYSQL *mysql) const noexcept
            {
                if (mysql)
                {
                    mysql_close(mysql);
                }
            }
        };

        /** @brief RAII handle for MYSQL connection using shared_ptr (supports weak_ptr) */
        using MySQLHandle = std::shared_ptr<MYSQL>;

        /**
         * @brief Factory function to create a MySQLHandle with the correct deleter
         *
         * Ensures MySQLDeleter is always used, preventing accidental creation of
         * a shared_ptr<MYSQL> with the default deleter (which would call delete
         * instead of mysql_close).
         */
        inline MySQLHandle makeMySQLHandle(MYSQL *mysql)
        {
            return MySQLHandle(mysql, MySQLDeleter{});
        }

} // namespace cpp_dbc::MySQL

#endif // USE_MYSQL
