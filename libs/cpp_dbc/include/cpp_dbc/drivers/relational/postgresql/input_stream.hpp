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
        // PostgreSQL implementation of InputStream
        class PostgreSQLInputStream : public InputStream
        {
        private:
            const std::vector<uint8_t> m_data;
            size_t m_position{0};

            static const char *validateAndEnd(const char *buffer, size_t length)
            {
                if (length > 0 && buffer == nullptr)
                    throw DBException("8KV3N7QW2FX9", "Null buffer passed to PostgreSQLInputStream", system_utils::captureCallStack());
                return buffer + length;
            }

        public:
            PostgreSQLInputStream(const char *buffer, size_t length)
                : m_data(buffer, validateAndEnd(buffer, length)) {}

            int read(uint8_t *buffer, size_t length) override
            {
                if (buffer == nullptr && length > 0)
                    return -1;

                if (m_position >= m_data.size())
                    return -1; // End of stream

                size_t bytesToRead = std::min({length, m_data.size() - m_position, static_cast<size_t>(INT_MAX)});
                std::memcpy(buffer, m_data.data() + m_position, bytesToRead);
                m_position += bytesToRead;
                return static_cast<int>(bytesToRead);
            }

            void skip(size_t n) override
            {
                size_t remaining = m_data.size() - m_position;
                m_position += std::min(n, remaining);
            }

            void close() override
            {
                // Nothing to do for memory stream
            }
        };

} // namespace cpp_dbc::PostgreSQL

#endif // USE_POSTGRESQL
