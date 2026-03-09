#pragma once

#include "../../../cpp_dbc.hpp"

#ifndef USE_MYSQL
#define USE_MYSQL 0
#endif

#if USE_MYSQL

#include <algorithm>
#include <climits>
#include <cstring>
#include <vector>

namespace cpp_dbc::MySQL
{
    /**
     * @brief MySQL-specific InputStream implementation for reading BLOB data
     *
     * Reads from an internal byte buffer populated from MySQL query results.
     * The buffer is copied on construction, so the source can be safely freed.
     *
     * @see InputStream, MySQLBlob
     */
    class MySQLInputStream : public InputStream
    {
        /**
         * @brief Private tag for the passkey idiom — enables std::make_shared
         * from static factory methods while keeping the constructor
         * effectively private (external code cannot construct PrivateCtorTag).
         */
        struct PrivateCtorTag
        {
            explicit PrivateCtorTag() = default;
        };

        // ── Member variables ──────────────────────────────────────────────────
        std::vector<uint8_t> m_data;
        size_t m_position{0};

        /**
         * @brief Flag indicating constructor initialization failed
         *
         * Set by the nothrow constructor when buffer validation fails.
         * Inspected by create(nothrow_t) to propagate the error via expected.
         */
        bool m_initFailed{false};

        /**
         * @brief Error captured when constructor initialization fails
         *
         * Holds the DBException that would have been thrown, for deferred delivery.
         */
        std::unique_ptr<DBException> m_initError{nullptr};

    public:
        // ── PrivateCtorTag constructor ────────────────────────────────────────
        /**
         * @brief Nothrow constructor — validates buffer and copies data.
         *
         * On failure (null buffer with non-zero length), sets m_initFailed
         * and m_initError instead of throwing.
         * Public for std::make_shared access, but effectively private:
         * external code cannot construct PrivateCtorTag.
         */
        MySQLInputStream(PrivateCtorTag, std::nothrow_t, const char *buffer, size_t length) noexcept
        {
            if (length > 0 && buffer == nullptr)
            {
                m_initFailed = true;
                m_initError = std::make_unique<DBException>("B10SNQSDSV85", "Null buffer passed to MySQLInputStream", system_utils::captureCallStack());
                return;
            }
            if (buffer != nullptr && length > 0)
            {
                m_data.assign(reinterpret_cast<const uint8_t *>(buffer),
                              reinterpret_cast<const uint8_t *>(buffer) + length);
            }
        }

        ~MySQLInputStream() override = default;

        // ====================================================================
        // THROWING API - Exception-based (requires __cpp_exceptions)
        // ====================================================================

#ifdef __cpp_exceptions
        static std::shared_ptr<MySQLInputStream> create(const char *buffer, size_t length)
        {
            auto r = create(std::nothrow, buffer, length);
            if (!r.has_value())
            {
                throw r.error();
            }
            return r.value();
        }

        void copyFrom(const MySQLInputStream &other)
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
        // NOTHROW FACTORIES — exception-free, always available
        // ====================================================================

        static cpp_dbc::expected<std::shared_ptr<MySQLInputStream>, DBException> create(std::nothrow_t, const char *buffer, size_t length) noexcept
        {
            // No try/catch: std::make_shared can only throw std::bad_alloc, which is a
            // death-sentence exception — the heap is exhausted and no meaningful recovery
            // is possible. Catching it would hide a catastrophic failure as a silent error
            // return. Letting std::terminate fire is safer and more debuggable.
            auto obj = std::make_shared<MySQLInputStream>(PrivateCtorTag{}, std::nothrow, buffer, length);
            if (obj->m_initFailed)
            {
                return cpp_dbc::unexpected(std::move(*obj->m_initError));
            }
            return obj;
        }

        // ====================================================================
        // NOTHROW VERSIONS - Exception-free API
        // ====================================================================

        cpp_dbc::expected<void, DBException> copyFrom(std::nothrow_t, const MySQLInputStream &other) noexcept
        {
            if (this == &other)
            {
                return {};
            }
            // 2026-03-08T00:00:00Z
            // Bug: m_position was reset to 0 instead of copying other.m_position,
            // causing the copy to diverge for partially-consumed streams.
            // Solution: Copy other.m_position to preserve the read position.
            m_data = other.m_data;
            m_position = other.m_position;
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

} // namespace cpp_dbc::MySQL

#endif // USE_MYSQL
