/**
 * Copyright 2025 Tomas R Moreno P <tomasr.morenop@gmail.com>. All Rights Reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * This file is part of the cpp_dbc project and is licensed under the GNU GPL v3.
 * See the LICENSE.md file in the project root for more information.
 *
 * @file memory_input_stream.hpp
 * @brief In-memory InputStream implementation for ScyllaDB binary data
 */

#pragma once

#include "../../../cpp_dbc.hpp"

#if USE_SCYLLADB

#include <algorithm>
#include <climits>
#include <cstring>
#include <vector>

#include "cpp_dbc/core/streams.hpp"

namespace cpp_dbc::ScyllaDB
{
        /**
         * @brief In-memory InputStream for reading ScyllaDB BLOB/binary data
         *
         * Wraps a `std::vector<uint8_t>` as a seekable InputStream, used
         * internally by ScyllaDBResultSet::getBinaryStream() to expose
         * binary column data through the standard InputStream interface.
         *
         * ```cpp
         * auto stream = rs->getBinaryStream("avatar");
         * std::vector<uint8_t> buf(1024);
         * int bytesRead = stream->read(buf.data(), buf.size());
         * stream->close();
         * ```
         *
         * @see ScyllaDBResultSet::getBinaryStream, InputStream
         */
        class ScyllaMemoryInputStream final : public cpp_dbc::InputStream
        {
        private:
            std::vector<uint8_t> m_data;
            size_t m_position{0};

        public:
            explicit ScyllaMemoryInputStream(std::vector<uint8_t> data) : m_data(std::move(data)) {}

            int read(uint8_t *buffer, size_t length) override
            {
                if (buffer == nullptr && length > 0)
                    return -1;

                if (m_position >= m_data.size())
                    return -1; // EOF

                size_t toRead = std::min({length, m_data.size() - m_position, static_cast<size_t>(INT_MAX)});
                if (toRead == 0)
                    return 0;
                std::memcpy(buffer, m_data.data() + m_position, toRead);
                m_position += toRead;
                return static_cast<int>(toRead);
            }

            void skip(size_t n) override
            {
                size_t remaining = m_data.size() - m_position;
                m_position += std::min(n, remaining);
            }

            void close() override
            {
                // Nothing to close
            }
        };
} // namespace cpp_dbc::ScyllaDB

#endif // USE_SCYLLADB
