#pragma once

#include "../../../cpp_dbc.hpp"

#if USE_MYSQL

#include <cstring>
#include <vector>

namespace cpp_dbc::MySQL
{
        // MySQL implementation of InputStream
        class MySQLInputStream : public InputStream
        {
        private:
            const std::vector<uint8_t> m_data;
            size_t m_position{0};

        public:
            MySQLInputStream(const char *buffer, size_t length)
                : m_data(buffer, buffer + length) {}

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

} // namespace cpp_dbc::MySQL

#endif // USE_MYSQL
