#pragma once

#include "../../../cpp_dbc.hpp"

#if USE_POSTGRESQL
#include <libpq-fe.h>
#include <memory>
#include <mutex>

namespace cpp_dbc::PostgreSQL
{
    /**
     * @brief Custom deleter for PGresult* to use with unique_ptr
     *
     * This deleter ensures that PQclear() is called automatically
     * when the unique_ptr goes out of scope, preventing memory leaks.
     */
    struct PGresultDeleter
    {
        void operator()(PGresult *result) const noexcept
        {
            if (result)
            {
                PQclear(result);
            }
        }
    };

    /**
     * @brief Custom deleter for PGconn* to use with shared_ptr
     *
     * This deleter ensures that PQfinish() is called automatically
     * when the shared_ptr reference count reaches zero, preventing resource leaks.
     */
    struct PGconnDeleter
    {
        void operator()(PGconn *conn) const noexcept
        {
            if (conn)
            {
                PQfinish(conn);
            }
        }
    };

    /**
     * @brief Type alias for the smart pointer managing PGresult
     *
     * Uses unique_ptr with custom deleter to ensure automatic cleanup
     * of PostgreSQL result sets, even in case of exceptions.
     */
    using PGresultHandle = std::unique_ptr<PGresult, PGresultDeleter>;

    /**
     * @brief Type alias for the smart pointer managing PGconn connection (shared_ptr for weak_ptr support)
     *
     * Uses shared_ptr so that PreparedStatements can use weak_ptr to safely
     * detect when the connection has been closed.
     * Note: The deleter is passed to the constructor, not as a template parameter.
     */
    using PGconnHandle = std::shared_ptr<PGconn>;

    /**
     * @brief Type alias for shared connection mutex
     *
     * @details
     * This shared_ptr<recursive_mutex> is shared between a PostgreSQLDBConnection and all
     * its PreparedStatements. This ensures that ALL operations that use the PGconn*
     * connection (including DEALLOCATE in PreparedStatement destructors) are
     * serialized through the same mutex.
     *
     * **THE PROBLEM IT SOLVES:**
     *
     * Without a shared mutex, PreparedStatement uses its own mutex (m_mutex) while
     * Connection uses m_connMutex. When a PreparedStatement destructor runs (calling
     * PQexec with DEALLOCATE), it only locks its own mutex - NOT the connection's mutex.
     * This allows the destructor to run concurrently with connection operations in
     * other threads (like pool validation queries), causing protocol errors and corruption.
     *
     * **HOW IT WORKS:**
     *
     * 1. Connection creates a shared mutex: m_connMutex = std::make_shared<std::recursive_mutex>()
     * 2. When creating a PreparedStatement, Connection passes its shared mutex
     * 3. PreparedStatement stores the same shared mutex
     * 4. ALL operations on both Connection and PreparedStatement lock the SAME mutex
     * 5. This includes PreparedStatement destructor calling PQexec(DEALLOCATE)
     * 6. Result: No race conditions possible
     */
    using SharedConnMutex = std::shared_ptr<std::recursive_mutex>;

} // namespace cpp_dbc::PostgreSQL

#endif // USE_POSTGRESQL
