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
        private:
            const std::vector<uint8_t> m_data;
            size_t m_position{0};

            static const uint8_t *validateAndEnd(const void *buffer, size_t length)
            {
                if (length > 0 && buffer == nullptr)
                    throw DBException("7WF2L5RQ8GT4", "Null buffer passed to FirebirdInputStream", system_utils::captureCallStack());
                return static_cast<const uint8_t *>(buffer) + length;
            }

        public:
            FirebirdInputStream(const void *buffer, size_t length)
                : m_data(static_cast<const uint8_t *>(buffer), validateAndEnd(buffer, length)) {}

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

} // namespace cpp_dbc::Firebird

#endif // USE_FIREBIRD
