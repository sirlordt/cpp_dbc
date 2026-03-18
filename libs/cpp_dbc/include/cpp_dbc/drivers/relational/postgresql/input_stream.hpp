#pragma once

#include "../../../cpp_dbc.hpp"

#ifndef USE_POSTGRESQL
#define USE_POSTGRESQL 0
#endif

#if USE_POSTGRESQL

#include <algorithm>
#include <climits>
#include <cstring>
#include <vector>

namespace cpp_dbc::PostgreSQL
{
    // Constants for large object access modes if not already defined by libpq
#ifndef INV_READ
    inline constexpr int INV_READ_VALUE = 0x00040000;
#define INV_READ INV_READ_VALUE
#endif

#ifndef INV_WRITE
    inline constexpr int INV_WRITE_VALUE = 0x00020000;
#define INV_WRITE INV_WRITE_VALUE
#endif
    /**
     * @brief PostgreSQL-specific InputStream implementation for reading BLOB data
     *
     * Reads from an internal byte buffer populated from PostgreSQL query results.
     * The buffer is copied on construction, so the source can be safely freed.
     *
     * @see InputStream, PostgreSQLBlob
     */
    class PostgreSQLInputStream : public InputStream
    {
        struct PrivateCtorTag
        {
            explicit PrivateCtorTag() = default;
        };

        std::vector<uint8_t> m_data;
        size_t m_position{0};

        bool m_initFailed{false};
        std::unique_ptr<DBException> m_initError{nullptr};

        static cpp_dbc::expected<const char *, DBException> validateAndEnd(std::nothrow_t, const char *buffer, size_t length) noexcept
        {
            if (length > 0 && buffer == nullptr)
            {
                return cpp_dbc::unexpected(DBException("71C2CH1H8E2J", "Null buffer passed to PostgreSQLInputStream", system_utils::captureCallStack()));
            }
            return buffer + length;
        }

    public:
        /**
         * @brief Construct an input stream from a PostgreSQL BLOB buffer
         * @param buffer Pointer to the BLOB data (may be null if length is 0)
         * @param length Size of the buffer in bytes
         *
         * The buffer data is copied into internal storage, so the source
         * buffer can be safely freed after construction.
         * On validation failure, sets m_initFailed and m_initError instead of throwing.
         */
        PostgreSQLInputStream(PrivateCtorTag, std::nothrow_t, const char *buffer, size_t length) noexcept
        {
            auto endResult = validateAndEnd(std::nothrow, buffer, length);
            if (!endResult.has_value())
            {
                m_initFailed = true;
                m_initError = std::make_unique<DBException>(std::move(endResult).error());
                return;
            }
            // Vector construction from range [buffer, endPtr) — can only throw bad_alloc (death sentence)
            m_data.assign(reinterpret_cast<const uint8_t *>(buffer),
                          reinterpret_cast<const uint8_t *>(endResult.value()));
        }

        ~PostgreSQLInputStream() override = default;

        PostgreSQLInputStream(const PostgreSQLInputStream &) = delete;
        PostgreSQLInputStream &operator=(const PostgreSQLInputStream &) = delete;
        PostgreSQLInputStream(PostgreSQLInputStream &&) = delete;
        PostgreSQLInputStream &operator=(PostgreSQLInputStream &&) = delete;

#ifdef __cpp_exceptions
        static std::shared_ptr<PostgreSQLInputStream> create(const char *buffer, size_t length)
        {
            auto r = create(std::nothrow, buffer, length);
            if (!r.has_value())
            {
                throw r.error();
            }
            return r.value();
        }
#endif // __cpp_exceptions

        static cpp_dbc::expected<std::shared_ptr<PostgreSQLInputStream>, DBException> create(std::nothrow_t, const char *buffer, size_t length) noexcept
        {
            // No try/catch: std::make_shared can only throw std::bad_alloc, which is a
            // death-sentence exception — the heap is exhausted and no meaningful recovery
            // is possible. The constructor is noexcept and captures errors in m_initFailed.
            auto obj = std::make_shared<PostgreSQLInputStream>(PrivateCtorTag{}, std::nothrow, buffer, length);
            if (obj->m_initFailed)
            {
                return cpp_dbc::unexpected(std::move(*obj->m_initError));
            }
            return obj;
        }

        // ====================================================================
        // THROWING API - Exception-based (requires __cpp_exceptions)
        // ====================================================================

#ifdef __cpp_exceptions

        void copyFrom(const PostgreSQLInputStream &other)
        {
            auto r = copyFrom(std::nothrow, other);
            if (!r.has_value())
            {
                throw r.error();
            }
        }

        int read(uint8_t *buffer, size_t length) override
        {
            auto r = read(std::nothrow, buffer, length);
            if (!r.has_value())
            {
                throw r.error();
            }
            return r.value();
        }

        void skip(size_t n) override
        {
            auto r = skip(std::nothrow, n);
            if (!r.has_value())
            {
                throw r.error();
            }
        }

        void close() override
        {
            auto r = close(std::nothrow);
            if (!r.has_value())
            {
                throw r.error();
            }
        }

#endif // __cpp_exceptions

        // ====================================================================
        // NOTHROW VERSIONS - Exception-free API
        // ====================================================================

        cpp_dbc::expected<void, DBException> copyFrom(std::nothrow_t, const PostgreSQLInputStream &other) noexcept
        {
            if (this == &other)
            {
                return {};
            }
            m_data = other.m_data;
            m_position = 0;
            m_initFailed = false;
            m_initError = nullptr;
            return {};
        }

        cpp_dbc::expected<int, DBException> read(std::nothrow_t, uint8_t *buffer, size_t length) noexcept override
        {
            if (buffer == nullptr && length > 0)
            {
                return -1;
            }
            if (m_position >= m_data.size())
            {
                return -1; // End of stream
            }
            size_t bytesToRead = std::min({length, m_data.size() - m_position, static_cast<size_t>(INT_MAX)});
            std::memcpy(buffer, m_data.data() + m_position, bytesToRead);
            m_position += bytesToRead;
            return static_cast<int>(bytesToRead);
        }

        cpp_dbc::expected<void, DBException> skip(std::nothrow_t, size_t n) noexcept override
        {
            size_t remaining = m_data.size() - m_position;
            m_position += std::min(n, remaining);
            return {};
        }

        cpp_dbc::expected<void, DBException> close(std::nothrow_t) noexcept override
        {
            // Nothing to do for memory stream
            return {};
        }
    };

} // namespace cpp_dbc::PostgreSQL

#endif // USE_POSTGRESQL
