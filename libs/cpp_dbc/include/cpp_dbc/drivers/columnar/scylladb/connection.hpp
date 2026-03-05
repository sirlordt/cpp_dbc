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
 * @file connection.hpp
 * @brief ScyllaDB connection implementation
 */

#pragma once

#include "handles.hpp"

#if USE_SCYLLADB

#include <memory>
#include <mutex>
#include <atomic>
#include <map>
#include <string>

#include "cpp_dbc/core/columnar/columnar_db_connection.hpp"
#include "cpp_dbc/core/db_exception.hpp"

namespace cpp_dbc::ScyllaDB
{
    /**
     * @brief ScyllaDB connection implementation
     *
     * Concrete ColumnarDBConnection for ScyllaDB/Cassandra databases.
     * Supports prepared statements, query execution, and lightweight
     * transactions (LWT). Uses the Cassandra C/C++ driver session
     * underneath.
     *
     * ```cpp
     * auto conn = std::dynamic_pointer_cast<cpp_dbc::ScyllaDB::ScyllaDBConnection>(
     *     cpp_dbc::DriverManager::getDBConnection(
     *         "cpp_dbc:scylladb://localhost:9042/mykeyspace", "", ""));
     * auto rs = conn->executeQuery("SELECT * FROM users");
     * while (rs->next()) {
     *     std::cout << rs->getString("name") << std::endl;
     * }
     * conn->close();
     * ```
     *
     * @see ScyllaDBDriver, ScyllaDBPreparedStatement, ScyllaDBResultSet
     */
    class ScyllaDBConnection final : public cpp_dbc::ColumnarDBConnection
    {
    private:
        /**
         * @brief Private tag for the passkey idiom — enables std::make_shared
         * from static factory methods while keeping the constructor
         * effectively private (external code cannot construct PrivateCtorTag).
         */
        struct PrivateCtorTag
        {
            explicit PrivateCtorTag() = default;
        };

        std::shared_ptr<CassCluster> m_cluster; // Shared to keep cluster config alive if needed
        std::shared_ptr<CassSession> m_session; // Shared for PreparedStatement weak_ptr
        std::string m_url;
        std::atomic<bool> m_closed{true};
        bool m_initFailed{false};
        DBException m_initError{"0J9B2L099DS6", "", {}};

#if DB_DRIVER_THREAD_SAFE
        mutable std::recursive_mutex m_connMutex;
#endif

    public:
        /**
         * @brief Nothrow constructor — contains all connection logic.
         *
         * Never throws — stores any error in m_initFailed/m_initError for the
         * caller (factory or delegating throwing constructor) to inspect.
         *
         * Public for std::make_shared access, but effectively private:
         * external code cannot construct PrivateCtorTag.
         */
        ScyllaDBConnection(PrivateCtorTag,
                           std::nothrow_t,
                           const std::string &host,
                           int port,
                           const std::string &keyspace,
                           const std::string &user,
                           const std::string &password,
                           const std::map<std::string, std::string> &options);

        ~ScyllaDBConnection() override;

        // ====================================================================
        // THROWING API — requires exception support
        // ====================================================================

#ifdef __cpp_exceptions
        static std::shared_ptr<ScyllaDBConnection>
        create(const std::string &host,
               int port,
               const std::string &keyspace,
               const std::string &user,
               const std::string &password,
               const std::map<std::string, std::string> &options = std::map<std::string, std::string>())
        {
            auto r = create(std::nothrow, host, port, keyspace, user, password, options);
            if (!r.has_value())
            {
                throw r.error();
            }
            return r.value();
        }

        // ColumnarDBConnection interface - throwing API (wrappers)

        void close() override;
        bool isClosed() const override;
        void returnToPool() override;
        bool isPooled() const override;
        std::string getURL() const override;
        void reset() override;
        bool ping() override;

        // ColumnarDBConnection interface
        std::shared_ptr<ColumnarDBPreparedStatement> prepareStatement(const std::string &query) override;
        std::shared_ptr<ColumnarDBResultSet> executeQuery(const std::string &query) override;
        uint64_t executeUpdate(const std::string &query) override;

        bool beginTransaction() override;
        void commit() override;
        void rollback() override;
        void prepareForPoolReturn() override;

#endif // __cpp_exceptions

        // ====================================================================
        // NOTHROW API — exception-free, always available
        // ====================================================================

        static cpp_dbc::expected<std::shared_ptr<ScyllaDBConnection>, DBException>
        create(std::nothrow_t,
               const std::string &host,
               int port,
               const std::string &keyspace,
               const std::string &user,
               const std::string &password,
               const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) noexcept
        {
            // The nothrow constructor stores init errors in m_initFailed/m_initError
            // rather than throwing, so no try/catch is needed here.
            auto obj = std::make_shared<ScyllaDBConnection>(
                PrivateCtorTag{}, std::nothrow, host, port, keyspace, user, password, options);
            if (obj->m_initFailed)
            {
                return cpp_dbc::unexpected(obj->m_initError);
            }
            return obj;
        }

        // DBConnection nothrow interface
        cpp_dbc::expected<void, DBException> close(std::nothrow_t) noexcept override;
        cpp_dbc::expected<bool, DBException> isClosed(std::nothrow_t) const noexcept override;
        cpp_dbc::expected<void, DBException> returnToPool(std::nothrow_t) noexcept override;
        cpp_dbc::expected<bool, DBException> isPooled(std::nothrow_t) const noexcept override;
        cpp_dbc::expected<std::string, DBException> getURL(std::nothrow_t) const noexcept override;
        cpp_dbc::expected<void, DBException> reset(std::nothrow_t) noexcept override;
        cpp_dbc::expected<bool, DBException> ping(std::nothrow_t) noexcept override;

        // ColumnarDBConnection nothrow interface
        cpp_dbc::expected<std::shared_ptr<ColumnarDBPreparedStatement>, DBException> prepareStatement(std::nothrow_t, const std::string &query) noexcept override;
        cpp_dbc::expected<std::shared_ptr<ColumnarDBResultSet>, DBException> executeQuery(std::nothrow_t, const std::string &query) noexcept override;
        cpp_dbc::expected<uint64_t, DBException> executeUpdate(std::nothrow_t, const std::string &query) noexcept override;
        cpp_dbc::expected<bool, DBException> beginTransaction(std::nothrow_t) noexcept override;
        cpp_dbc::expected<void, DBException> commit(std::nothrow_t) noexcept override;
        cpp_dbc::expected<void, DBException> rollback(std::nothrow_t) noexcept override;
        cpp_dbc::expected<void, DBException> prepareForPoolReturn(std::nothrow_t) noexcept override;
    };
} // namespace cpp_dbc::ScyllaDB

#endif // USE_SCYLLADB
