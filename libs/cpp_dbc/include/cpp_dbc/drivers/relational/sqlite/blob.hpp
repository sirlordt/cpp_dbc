#pragma once

#include "../../../cpp_dbc.hpp"
#include "../../../blob.hpp"

#ifndef USE_SQLITE
#define USE_SQLITE 0
#endif

#if USE_SQLITE
#include <cctype>
#include <cstring>
#include <memory>
#include <sqlite3.h>
#include <string>
#include <vector>

namespace cpp_dbc::SQLite
{
    /**
     * @brief SQLite Blob implementation with lazy loading from database
     *
     * Extends MemoryBlob with database-backed lazy loading via sqlite3 APIs.
     * Uses weak_ptr to safely detect when the connection has been closed.
     *
     * @see MemoryBlob, SQLiteDBResultSet
     */
    class SQLiteBlob : public MemoryBlob
    {
        // ── PrivateCtorTag — prevents direct construction; use create() ──
        struct PrivateCtorTag
        {
            explicit PrivateCtorTag() = default;
        };

        /**
         * @brief Safe weak reference to SQLite connection - detects when connection is closed
         *
         * Uses weak_ptr to safely detect when the connection has been closed,
         * preventing use-after-free errors. The connection is managed by SQLiteConnection
         * using shared_ptr with custom deleter.
         */
        std::weak_ptr<sqlite3> m_conn;
        std::string m_tableName;
        std::string m_columnName;
        std::string m_rowId;
        mutable bool m_loaded{false};

        // ── Construction state ────────────────────────────────────────────────
        bool m_initFailed{false};
        std::unique_ptr<DBException> m_initError{nullptr};

        /**
         * @brief Helper method to get sqlite3* safely, returns unexpected if connection is closed
         * @return The sqlite3 connection pointer or error
         */
        cpp_dbc::expected<sqlite3 *, DBException> getSQLiteConnection(std::nothrow_t) const noexcept;

        /**
         * @brief Validate SQL identifier to prevent injection attacks
         * @param identifier The table or column name to validate
         * @return void on success, or error if the identifier contains invalid characters
         *
         * Only allows alphanumeric characters and underscores to prevent SQL injection.
         * This follows the cpp_dbc project convention for schema name validation.
         */
        static cpp_dbc::expected<void, DBException> validateIdentifier(std::nothrow_t, const std::string &identifier) noexcept;

    public:
        // ── Public nothrow constructors (guarded by PrivateCtorTag) ─────────────
        // No try/catch: all operations are death-sentence (std::bad_alloc from
        // string/vector copies) or trivially noexcept (weak_ptr copy, bool assign).

        /** @brief Construct an empty BLOB for in-memory use */
        SQLiteBlob(PrivateCtorTag, std::nothrow_t, std::shared_ptr<sqlite3> db) noexcept
            : m_conn(db), m_loaded(true)
        {
            // Intentionally empty — initialization done in member initializer list
        }

        /** @brief Construct a lazy-loading BLOB from a database row */
        SQLiteBlob(PrivateCtorTag, std::nothrow_t, std::shared_ptr<sqlite3> db,
                   const std::string &tableName, const std::string &columnName,
                   const std::string &rowId) noexcept
            : m_conn(db), m_tableName(tableName), m_columnName(columnName),
              m_rowId(rowId), m_loaded(false)
        {
            // Intentionally empty — initialization done in member initializer list
        }

        /** @brief Construct a BLOB from existing binary data */
        SQLiteBlob(PrivateCtorTag, std::nothrow_t, std::shared_ptr<sqlite3> db,
                   const std::vector<uint8_t> &initialData) noexcept
            : MemoryBlob(initialData), m_conn(db), m_loaded(true)
        {
            // Intentionally empty — initialization done in member initializer list
        }

        ~SQLiteBlob() override = default;

        SQLiteBlob(const SQLiteBlob &) = delete;
        SQLiteBlob &operator=(const SQLiteBlob &) = delete;

        // ====================================================================
        // THROWING API - Exception-based (requires __cpp_exceptions)
        // ====================================================================

#ifdef __cpp_exceptions

        // ── Throwing static factories (delegate to nothrow) ──────────────────

        static std::shared_ptr<SQLiteBlob> create(std::shared_ptr<sqlite3> db);

        static std::shared_ptr<SQLiteBlob> create(std::shared_ptr<sqlite3> db,
                                                   const std::string &tableName, const std::string &columnName, const std::string &rowId);

        static std::shared_ptr<SQLiteBlob> create(std::shared_ptr<sqlite3> db,
                                                   const std::vector<uint8_t> &initialData);

        // ── Throwing public methods ──────────────────────────────────────────

        void copyFrom(const SQLiteBlob &other);
        void ensureLoaded() const;
        size_t length() const override;
        std::vector<uint8_t> getBytes(size_t pos, size_t length) const override;
        std::shared_ptr<InputStream> getBinaryStream() const override;
        std::shared_ptr<OutputStream> setBinaryStream(size_t pos) override;
        void setBytes(size_t pos, const std::vector<uint8_t> &bytes) override;
        void setBytes(size_t pos, const uint8_t *bytes, size_t length) override;
        void truncate(size_t len) override;

        /**
         * @brief Save the BLOB data back to the database
         * @throws DBException if the connection is closed or saving fails
         */
        void save();

        void free() override;

#endif // __cpp_exceptions

        // ====================================================================
        // NOTHROW API - Exception-free (always available)
        // ====================================================================

        // ── Nothrow static factories ─────────────────────────────────────────
        // std::make_shared may throw std::bad_alloc — death sentence, no try/catch.

        static cpp_dbc::expected<std::shared_ptr<SQLiteBlob>, DBException> create(std::nothrow_t, std::shared_ptr<sqlite3> db) noexcept;

        static cpp_dbc::expected<std::shared_ptr<SQLiteBlob>, DBException> create(std::nothrow_t, std::shared_ptr<sqlite3> db,
                                                                                  const std::string &tableName, const std::string &columnName, const std::string &rowId) noexcept;

        static cpp_dbc::expected<std::shared_ptr<SQLiteBlob>, DBException> create(std::nothrow_t, std::shared_ptr<sqlite3> db,
                                                                                  const std::vector<uint8_t> &initialData) noexcept;

        // ── Nothrow public methods ───────────────────────────────────────────

        /**
         * @brief Check if the database connection is still valid
         * @return true if connection is valid, false if it has been closed
         */
        bool isConnectionValid() const noexcept override
        {
            return !m_conn.expired();
        }

        cpp_dbc::expected<void, DBException> copyFrom(std::nothrow_t, const SQLiteBlob &other) noexcept;

        /**
         * @brief Load the BLOB data from the database if not already loaded
         * @return void on success, or error if the connection is closed or loading fails
         */
        cpp_dbc::expected<void, DBException> ensureLoaded(std::nothrow_t) const noexcept;

        // ── Nothrow Blob overrides ───────────────────────────────────────────

        cpp_dbc::expected<size_t, DBException> length(std::nothrow_t) const noexcept override;
        cpp_dbc::expected<std::vector<uint8_t>, DBException> getBytes(std::nothrow_t, size_t pos, size_t length) const noexcept override;
        cpp_dbc::expected<std::shared_ptr<InputStream>, DBException> getBinaryStream(std::nothrow_t) const noexcept override;
        cpp_dbc::expected<std::shared_ptr<OutputStream>, DBException> setBinaryStream(std::nothrow_t, size_t pos) noexcept override;
        cpp_dbc::expected<void, DBException> setBytes(std::nothrow_t, size_t pos, const std::vector<uint8_t> &bytes) noexcept override;
        cpp_dbc::expected<void, DBException> setBytes(std::nothrow_t, size_t pos, const uint8_t *bytes, size_t length) noexcept override;
        cpp_dbc::expected<void, DBException> truncate(std::nothrow_t, size_t len) noexcept override;
        cpp_dbc::expected<void, DBException> save(std::nothrow_t) noexcept;
        cpp_dbc::expected<void, DBException> free(std::nothrow_t) noexcept override;
    };

} // namespace cpp_dbc::SQLite

#endif // USE_SQLITE
