#pragma once

#include "../../../cpp_dbc.hpp"

#ifndef USE_FIREBIRD
#define USE_FIREBIRD 0
#endif

#if USE_FIREBIRD
#include <ibase.h>
#include <memory>
#include <mutex>
#include <string>
#include <cstring>

namespace cpp_dbc::Firebird
{
    // Forward declarations
    class FirebirdDBConnection;
    class FirebirdDBPreparedStatement;

#if DB_DRIVER_THREAD_SAFE
    /**
     * @brief Shared mutex type for connection and prepared statements
     *
     * This shared_ptr to a recursive_mutex ensures that the Connection and all its
     * PreparedStatements share the SAME mutex. This prevents race conditions when:
     * - PreparedStatement destructor calls isc_dsql_free_statement()
     * - Another thread uses the connection for queries or pool validation
     *
     * Without a shared mutex, concurrent access to the Firebird database handle
     * can cause protocol errors and memory corruption.
     */
    using SharedConnMutex = std::shared_ptr<std::recursive_mutex>;
#endif

    /**
     * @brief Helper function to interpret Firebird status vector
     * @param status The status vector from Firebird API calls
     * @return Error message string with detailed error information
     */
    inline std::string interpretStatusVector(const ISC_STATUS_ARRAY status)
    {
        std::string result;

        // First, get the SQL code for more specific error information
        ISC_LONG sqlcode = isc_sqlcode(status);
        if (sqlcode != 0)
        {
            char sqlMsg[256];
            isc_sql_interprete(static_cast<short>(sqlcode), sqlMsg, sizeof(sqlMsg));
            result = "SQLCODE " + std::to_string(sqlcode) + ": " + std::string(sqlMsg);
        }

        // Then get the detailed error messages from the status vector using fb_interpret
        // This is the primary and most reliable source of error information
        char buffer[1024];
        const ISC_STATUS *pvector = status;
        std::string details;

        while (fb_interpret(buffer, sizeof(buffer), &pvector))
        {
            if (!details.empty())
                details += " - ";
            details += buffer;
        }

        // Combine the messages
        if (!details.empty())
        {
            if (!result.empty())
                result += " | ";
            result += details;
            // fb_interpret gave us a good message, return it
            return result;
        }

        // Final fallback - only if fb_interpret didn't give us anything
        if (result.empty())
        {
            result = "Unknown Firebird error (status[0]=" + std::to_string(status[0]) +
                     ", status[1]=" + std::to_string(status[1]) + ")";
        }

        return result;
    }

    /**
     * @brief Custom deleter for Firebird statement handle
     * Note: This deleter only frees the pointer wrapper, NOT the statement itself.
     * The statement is freed by the PreparedStatement::close() or ResultSet::close() methods.
     */
    struct FirebirdStmtDeleter
    {
        void operator()(isc_stmt_handle *stmt) const noexcept
        {
            // Only delete the pointer wrapper, don't free the statement
            // The statement is managed by PreparedStatement or ResultSet
            delete stmt;
        }
    };

    /**
     * @brief Type alias for the smart pointer managing Firebird statement
     */
    using FirebirdStmtHandle = std::unique_ptr<isc_stmt_handle, FirebirdStmtDeleter>;

    /**
     * @brief Custom deleter for XSQLDA structure
     */
    struct XSQLDADeleter
    {
        void operator()(XSQLDA *sqlda) const noexcept
        {
            if (sqlda)
            {
                free(sqlda);
            }
        }
    };

    /**
     * @brief Type alias for the smart pointer managing XSQLDA
     */
    using XSQLDAHandle = std::unique_ptr<XSQLDA, XSQLDADeleter>;

    /**
     * @brief Custom deleter for transaction handle (non-owning, just for type safety)
     * Note: Transaction handles are managed by FirebirdConnection, not by this deleter
     */
    struct FirebirdTrDeleter
    {
        void operator()(isc_tr_handle *tr) const noexcept
        {
            // Transaction handles are managed by FirebirdConnection
            // This deleter only frees the pointer wrapper, not the transaction itself
            delete tr;
        }
    };

    /**
     * @brief Type alias for transaction handle wrapper
     */
    using FirebirdTrHandle = std::unique_ptr<isc_tr_handle, FirebirdTrDeleter>;

    /**
     * @brief Custom deleter for Firebird database handle
     */
    struct FirebirdDbDeleter
    {
        void operator()(isc_db_handle *db) const noexcept
        {
            if (db)
            {
                if (*db)
                {
                    ISC_STATUS_ARRAY status;
                    isc_detach_database(status, db);
                }
                delete db;
            }
        }
    };

    /**
     * @brief Type alias for the smart pointer managing Firebird connection
     */
    using FirebirdDbHandle = std::shared_ptr<isc_db_handle>;

} // namespace cpp_dbc::Firebird

#endif // USE_FIREBIRD
