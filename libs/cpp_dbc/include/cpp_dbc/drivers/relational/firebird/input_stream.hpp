#pragma once

#include "../../../cpp_dbc.hpp"

#ifndef USE_FIREBIRD
#define USE_FIREBIRD 0
#endif

#if USE_FIREBIRD
#include <cstring>
#include <vector>

namespace cpp_dbc::Firebird
{
        // Firebird implementation of InputStream
        class FirebirdInputStream : public InputStream
        {
        private:
            const std::vector<uint8_t> m_data;
            size_t m_position{0};

        public:
            FirebirdInputStream(const void *buffer, size_t length)
                : m_data(
                    length > 0 && buffer == nullptr
                        ? throw DBException("7WF2L5RQ8GT4", "Null buffer passed to FirebirdInputStream", system_utils::captureCallStack())
                        : static_cast<const uint8_t *>(buffer),
                    static_cast<const uint8_t *>(buffer) + length) {}

            int read(uint8_t *buffer, size_t length) override
            {
                if (m_position >= m_data.size())
                    return -1; // End of stream

                size_t bytesToRead = std::min(length, m_data.size() - m_position);
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

} // namespace cpp_dbc::Firebird

#endif // USE_FIREBIRD
