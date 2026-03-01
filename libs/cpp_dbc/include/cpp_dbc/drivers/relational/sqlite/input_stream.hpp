#pragma once

#include "../../../cpp_dbc.hpp"

#ifndef USE_SQLITE
#define USE_SQLITE 0
#endif

#if USE_SQLITE
#include <algorithm>
#include <climits>
#include <cstring>
#include <vector>

namespace cpp_dbc::SQLite
{
    /**
     * @brief SQLite-specific InputStream implementation for reading BLOB data
     *
     * Reads from an internal byte buffer populated from SQLite query results.
     * The buffer is copied on construction, so the source can be safely freed.
     *
     * @see InputStream, SQLiteBlob
     */
    class SQLiteInputStream : public InputStream
    {
    private:
        std::vector<uint8_t> m_data;
        size_t m_position{0};

        static cpp_dbc::expected<const uint8_t *, DBException> validateAndEnd(std::nothrow_t, const void *buffer, size_t length) noexcept
        {
            if (length > 0 && buffer == nullptr)
            {
                return cpp_dbc::unexpected(DBException("VJ52I7YQ3LB1", "Null buffer passed to SQLiteInputStream", system_utils::captureCallStack()));
            }
            return static_cast<const uint8_t *>(buffer) + length;
        }

        static const uint8_t *validateAndEnd(const void *buffer, size_t length)
        {
            auto r = validateAndEnd(std::nothrow, buffer, length);
            if (!r.has_value())
            {
                throw r.error();
            }
            return r.value();
        }

    public:
        SQLiteInputStream(const void *buffer, size_t length)
            : m_data(static_cast<const uint8_t *>(buffer), validateAndEnd(buffer, length)) {}

        static cpp_dbc::expected<std::shared_ptr<SQLiteInputStream>, DBException> create(std::nothrow_t, const void *buffer, size_t length) noexcept
        {
            try
            {
                return std::make_shared<SQLiteInputStream>(buffer, length);
            }
            catch (const DBException &ex)
            {
                return cpp_dbc::unexpected(ex);
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("FA81AE85ZCSQ", ex.what(), system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("HHTU4ZAX05S1", "Unknown error creating SQLiteInputStream", system_utils::captureCallStack()));
            }
        }

        static std::shared_ptr<SQLiteInputStream> create(const void *buffer, size_t length)
        {
            auto r = create(std::nothrow, buffer, length);
            if (!r.has_value())
            {
                throw r.error();
            }
            return r.value();
        }

        cpp_dbc::expected<void, DBException> copyFrom(std::nothrow_t, const SQLiteInputStream &other) noexcept
        {
            if (this == &other)
            {
                return {};
            }
            m_data = other.m_data;
            m_position = 0;
            return {};
        }

        void copyFrom(const SQLiteInputStream &other)
        {
            auto r = copyFrom(std::nothrow, other);
            if (!r.has_value())
            {
                throw r.error();
            }
        }

        // ====================================================================
        // THROWING API - Exception-based (requires __cpp_exceptions)
        // ====================================================================

#ifdef __cpp_exceptions

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

} // namespace cpp_dbc::SQLite

#endif // USE_SQLITE
