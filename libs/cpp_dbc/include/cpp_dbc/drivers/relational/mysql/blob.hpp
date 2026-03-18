#pragma once

#include "../../../cpp_dbc.hpp"
#include "../../../blob.hpp"

#if USE_MYSQL
#include <cctype>
#include <memory>
#include <cstring>

namespace cpp_dbc::MySQL
{
    class MySQLDBConnection;  // Forward declaration for weak_ptr and friend
    class MySQLConnectionLock; // Forward declaration for friend

    /**
     * @brief MySQL implementation of Blob with database-backed lazy loading
     *
     * Extends MemoryBlob with database-backed lazy loading via MySQL C API.
     * Synchronizes DB access through the parent connection's mutex via
     * weak_ptr<MySQLDBConnection>, following the same pattern as
     * PreparedStatement and ResultSet.
     *
     * @see MemoryBlob, MySQLDBResultSet, MySQLDBConnection
     */
    class MySQLBlob : public MemoryBlob
    {
        friend class MySQLConnectionLock;

        // ── PrivateCtorTag — prevents direct construction; use create() ──
        struct PrivateCtorTag
        {
            explicit PrivateCtorTag() = default;
        };

        using MemoryBlob::copyFrom; // Unhide base-class copyFrom overloads (cpp:S1242)

        // ── Member variables ──────────────────────────────────────────────────

        /**
         * @brief Weak reference to parent connection for mutex acquisition
         *
         * Used by MySQLConnectionLock (RAII helper) to acquire the connection's
         * mutex. When expired, the blob operates in pure in-memory mode
         * (no DB I/O possible). The blob does not need to know the mutex type
         * or origin — it acquires it through the connection, same as
         * PreparedStatement and ResultSet.
         */
        std::weak_ptr<MySQLDBConnection> m_connection;

        std::string m_tableName;
        std::string m_columnName;
        std::string m_whereClause;
        mutable bool m_loaded{false};
        mutable std::atomic<bool> m_closed{false};

        // ── Construction state ────────────────────────────────────────────────
        bool m_initFailed{false};
        std::unique_ptr<DBException> m_initError{nullptr};

        // ── Private helpers ───────────────────────────────────────────────────

        /**
         * @brief Get MYSQL* safely through the connection
         * @return Raw pointer to the MYSQL handle or error if connection is closed
         *
         * Must be called under the connection lock (via MYSQL_STMT_LOCK_OR_RETURN).
         */
        cpp_dbc::expected<MYSQL *, DBException> getMySQLHandle(std::nothrow_t) const noexcept;

        /**
         * @brief Validate that a database identifier contains only safe characters
         * @param identifier The identifier to validate (table name, column name)
         * @return unexpected(DBException) if the identifier contains disallowed characters
         */
        static cpp_dbc::expected<void, DBException> validateIdentifier(std::nothrow_t, const std::string &identifier) noexcept;

    public:
        // ── Public nothrow constructors (guarded by PrivateCtorTag) ─────────────
        // No try/catch: all operations are death-sentence (std::bad_alloc from
        // string/vector copies) or trivially noexcept (weak_ptr copy, bool assign).

        /** @brief Construct an empty BLOB for in-memory use */
        MySQLBlob(PrivateCtorTag, std::nothrow_t,
                  std::weak_ptr<MySQLDBConnection> conn) noexcept
            : m_loaded(true)
        {
            m_connection = std::move(conn);
        }

        /** @brief Construct a lazy-loading BLOB from a database row */
        MySQLBlob(PrivateCtorTag, std::nothrow_t,
                  std::weak_ptr<MySQLDBConnection> conn,
                  const std::string &tableName, const std::string &columnName,
                  const std::string &whereClause) noexcept
            : m_tableName(tableName), m_columnName(columnName),
              m_whereClause(whereClause)
        {
            m_connection = std::move(conn);
        }

        /** @brief Construct a BLOB from existing binary data */
        MySQLBlob(PrivateCtorTag, std::nothrow_t,
                  std::weak_ptr<MySQLDBConnection> conn,
                  const std::vector<uint8_t> &initialData) noexcept
            : MemoryBlob(initialData), m_loaded(true)
        {
            m_connection = std::move(conn);
        }

        ~MySQLBlob() override = default;

        MySQLBlob(const MySQLBlob &) = delete;
        MySQLBlob &operator=(const MySQLBlob &) = delete;

        // ====================================================================
        // THROWING API - Exception-based (requires __cpp_exceptions)
        // ====================================================================

#ifdef __cpp_exceptions

        // ── Throwing static factories (delegate to nothrow) ──────────────────

        static std::shared_ptr<MySQLBlob> create(std::weak_ptr<MySQLDBConnection> conn);

        static std::shared_ptr<MySQLBlob> create(std::weak_ptr<MySQLDBConnection> conn,
                                                  const std::string &tableName, const std::string &columnName,
                                                  const std::string &whereClause);

        static std::shared_ptr<MySQLBlob> create(std::weak_ptr<MySQLDBConnection> conn,
                                                  const std::vector<uint8_t> &initialData);

        // ── Throwing public methods ──────────────────────────────────────────

        void copyFrom(const MySQLBlob &other);
        void ensureLoaded() const;
        size_t length() const override;
        std::vector<uint8_t> getBytes(size_t pos, size_t length) const override;
        std::shared_ptr<InputStream> getBinaryStream() const override;
        std::shared_ptr<OutputStream> setBinaryStream(size_t pos) override;
        void setBytes(size_t pos, const std::vector<uint8_t> &bytes) override;
        void setBytes(size_t pos, const uint8_t *bytes, size_t length) override;
        void truncate(size_t len) override;
        void save();
        void free() override;

#endif // __cpp_exceptions

        // ====================================================================
        // NOTHROW API - Exception-free (always available)
        // ====================================================================

        // ── Nothrow static factories ─────────────────────────────────────────
        // std::make_shared may throw std::bad_alloc — death sentence, no try/catch.

        static cpp_dbc::expected<std::shared_ptr<MySQLBlob>, DBException> create(std::nothrow_t,
                                                                                  std::weak_ptr<MySQLDBConnection> conn) noexcept;

        static cpp_dbc::expected<std::shared_ptr<MySQLBlob>, DBException> create(std::nothrow_t,
                                                                                  std::weak_ptr<MySQLDBConnection> conn,
                                                                                  const std::string &tableName, const std::string &columnName,
                                                                                  const std::string &whereClause) noexcept;

        static cpp_dbc::expected<std::shared_ptr<MySQLBlob>, DBException> create(std::nothrow_t,
                                                                                  std::weak_ptr<MySQLDBConnection> conn,
                                                                                  const std::vector<uint8_t> &initialData) noexcept;

        // ── Nothrow public methods ───────────────────────────────────────────

        bool isConnectionValid() const noexcept override
        {
            return !m_connection.expired();
        }

        cpp_dbc::expected<void, DBException> copyFrom(std::nothrow_t, const MySQLBlob &other) noexcept;
        cpp_dbc::expected<void, DBException> ensureLoaded(std::nothrow_t) const noexcept;
        cpp_dbc::expected<void, DBException> save(std::nothrow_t) noexcept;

        // ── Nothrow Blob overrides ───────────────────────────────────────────

        cpp_dbc::expected<size_t, DBException> length(std::nothrow_t) const noexcept override;
        cpp_dbc::expected<std::vector<uint8_t>, DBException> getBytes(std::nothrow_t, size_t pos, size_t length) const noexcept override;
        cpp_dbc::expected<std::shared_ptr<InputStream>, DBException> getBinaryStream(std::nothrow_t) const noexcept override;
        cpp_dbc::expected<std::shared_ptr<OutputStream>, DBException> setBinaryStream(std::nothrow_t, size_t pos) noexcept override;
        cpp_dbc::expected<void, DBException> setBytes(std::nothrow_t, size_t pos, const std::vector<uint8_t> &bytes) noexcept override;
        cpp_dbc::expected<void, DBException> setBytes(std::nothrow_t, size_t pos, const uint8_t *bytes, size_t length) noexcept override;
        cpp_dbc::expected<void, DBException> truncate(std::nothrow_t, size_t len) noexcept override;
        cpp_dbc::expected<void, DBException> free(std::nothrow_t) noexcept override;
    };

} // namespace cpp_dbc::MySQL

#endif // USE_MYSQL
