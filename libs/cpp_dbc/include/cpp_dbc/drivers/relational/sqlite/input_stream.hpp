#pragma once

#include "../../../cpp_dbc.hpp"

#ifndef USE_SQLITE
#define USE_SQLITE 0
#endif

#if USE_SQLITE
#include <climits>
#include <cstring>
#include <vector>

namespace cpp_dbc::SQLite
{
        // SQLite implementation of InputStream
        class SQLiteInputStream : public InputStream
        {
        private:
            const std::vector<uint8_t> m_data;
            size_t m_position{0};

        public:
            SQLiteInputStream(const void *buffer, size_t length)
                : m_data(
                    length > 0 && buffer == nullptr
                        ? throw DBException("3YC6H9DK1NX7", "Null buffer passed to SQLiteInputStream", system_utils::captureCallStack())
                        : static_cast<const uint8_t *>(buffer),
                    static_cast<const uint8_t *>(buffer) + length) {}

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
                m_position = std::min(m_position + n, m_data.size());
            }

            void close() override
            {
                // Nothing to do for memory stream
            }
        };

} // namespace cpp_dbc::SQLite

#endif // USE_SQLITE
