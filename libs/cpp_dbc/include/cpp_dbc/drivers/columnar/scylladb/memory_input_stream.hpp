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

            static cpp_dbc::expected<std::shared_ptr<ScyllaMemoryInputStream>, DBException> create(std::nothrow_t, std::vector<uint8_t> data) noexcept
            {
                return std::make_shared<ScyllaMemoryInputStream>(std::move(data));
            }

            static std::shared_ptr<ScyllaMemoryInputStream> create(std::vector<uint8_t> data)
            {
                auto r = create(std::nothrow, std::move(data));
                if (!r.has_value()) { throw r.error(); }
                return r.value();
            }

            cpp_dbc::expected<void, DBException> copyFrom(std::nothrow_t, const ScyllaMemoryInputStream &other) noexcept
            {
                if (this == &other) { return {}; }
                m_data = other.m_data;
                m_position = 0;
                return {};
            }

            void copyFrom(const ScyllaMemoryInputStream &other)
            {
                auto r = copyFrom(std::nothrow, other);
                if (!r.has_value()) { throw r.error(); }
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
                    return -1; // EOF
                }
                size_t toRead = std::min({length, m_data.size() - m_position, static_cast<size_t>(INT_MAX)});
                if (toRead == 0)
                {
                    return 0;
                }
                std::memcpy(buffer, m_data.data() + m_position, toRead);
                m_position += toRead;
                return static_cast<int>(toRead);
            }

            cpp_dbc::expected<void, DBException> skip(std::nothrow_t, size_t n) noexcept override
            {
                size_t remaining = m_data.size() - m_position;
                m_position += std::min(n, remaining);
                return {};
            }

            cpp_dbc::expected<void, DBException> close(std::nothrow_t) noexcept override
            {
                // Nothing to close
                return {};
            }
        };
} // namespace cpp_dbc::ScyllaDB

#endif // USE_SCYLLADB
