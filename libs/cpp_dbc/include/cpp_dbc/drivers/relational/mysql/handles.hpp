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

        // Custom deleter for MYSQL_STMT* to use with unique_ptr
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

        // Type alias for the smart pointer managing MYSQL_STMT
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

        // Custom deleter for MYSQL* to use with shared_ptr
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

        // Type alias for the smart pointer managing MYSQL connection (shared_ptr for weak_ptr support)
        // Note: When creating a MySQLHandle, pass MySQLDeleter{} as the second argument:
        //       MySQLHandle(mysql_ptr, MySQLDeleter{})
        using MySQLHandle = std::shared_ptr<MYSQL>;

} // namespace cpp_dbc::MySQL

#endif // USE_MYSQL
