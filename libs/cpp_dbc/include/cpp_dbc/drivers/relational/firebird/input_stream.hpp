#pragma once

#include "../../../cpp_dbc.hpp"

#ifndef USE_FIREBIRD
#define USE_FIREBIRD 0
#endif

#if USE_FIREBIRD
#include <algorithm>
#include <climits>
#include <cstring>
#include <vector>

namespace cpp_dbc::Firebird
{
    /**
     * @brief Firebird-specific InputStream implementation for reading BLOB data
     *
     * Reads from an internal byte buffer populated from Firebird BLOB segments.
     * The buffer is copied on construction, so the source can be safely freed.
     *
     * @see InputStream, FirebirdBlob
     */
    class FirebirdInputStream : public InputStream
    {
        // ── PrivateCtorTag — prevents direct construction; use create() ──
        struct PrivateCtorTag
        {
            explicit PrivateCtorTag() = default;
        };

        std::vector<uint8_t> m_data;
        size_t m_position{0};

        // ── Construction state ────────────────────────────────────────────────
        bool m_initFailed{false};
        std::unique_ptr<DBException> m_initError{nullptr};

    public:
        // ── Public nothrow constructor (guarded by PrivateCtorTag) ─────────────
        // No try/catch: vector copy from valid range is death-sentence only (bad_alloc).
        // Validation failure is captured in m_initFailed/m_initError.
        FirebirdInputStream(PrivateCtorTag, std::nothrow_t, const void *buffer, size_t length) noexcept
        {
            if (length > 0 && buffer == nullptr)
            {
                m_initFailed = true;
                m_initError = std::make_unique<DBException>("EBAO9P4HU5U9",
                    "Null buffer passed to FirebirdInputStream",
                    system_utils::captureCallStack());
                return;
            }
            if (length > 0)
            {
                auto begin = static_cast<const uint8_t *>(buffer);
                m_data.assign(begin, begin + length);
            }
        }

        ~FirebirdInputStream() override = default;

        FirebirdInputStream(const FirebirdInputStream &) = delete;
        FirebirdInputStream &operator=(const FirebirdInputStream &) = delete;

        // ====================================================================
        // THROWING API - Exception-based (requires __cpp_exceptions)
        // ====================================================================

#ifdef __cpp_exceptions

        static std::shared_ptr<FirebirdInputStream> create(const void *buffer, size_t length)
        {
            auto r = create(std::nothrow, buffer, length);
            if (!r.has_value())
            {
                throw r.error();
            }
            return r.value();
        }

        void copyFrom(const FirebirdInputStream &other)
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
        // NOTHROW API - Exception-free (always available)
        // ====================================================================

        // ── Nothrow static factory ───────────────────────────────────────────
        // std::make_shared may throw std::bad_alloc — death sentence, no try/catch.
        static cpp_dbc::expected<std::shared_ptr<FirebirdInputStream>, DBException> create(std::nothrow_t, const void *buffer, size_t length) noexcept
        {
            auto obj = std::make_shared<FirebirdInputStream>(PrivateCtorTag{}, std::nothrow, buffer, length);
            if (obj->m_initFailed)
            {
                return cpp_dbc::unexpected(std::move(*obj->m_initError));
            }
            return obj;
        }

        cpp_dbc::expected<void, DBException> copyFrom(std::nothrow_t, const FirebirdInputStream &other) noexcept
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

} // namespace cpp_dbc::Firebird

#endif // USE_FIREBIRD
