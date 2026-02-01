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
 * @file prepared_statement_01.cpp
 * @brief ScyllaDB database driver implementation - ScyllaDBPreparedStatement throwing methods
 */

#include "cpp_dbc/drivers/columnar/driver_scylladb.hpp"

#if USE_SCYLLADB

#include <array>
#include <cctype>
#include <cstring>
#include <algorithm>
#include <sstream>
#include <iostream>
#include <thread>
#include <chrono>
#include <iomanip>
#include "cpp_dbc/common/system_utils.hpp"
#include "scylladb_internal.hpp"

namespace cpp_dbc::ScyllaDB
{
    // ====================================================================
    // ScyllaDBPreparedStatement
    // ====================================================================

    // Private methods

    void ScyllaDBPreparedStatement::checkSession()
    {
        if (m_session.expired())
        {
            SCYLLADB_DEBUG("ScyllaDBPreparedStatement::checkSession - Session is closed");
            throw DBException("A2B3C4D5E6F7", "Session is closed", system_utils::captureCallStack());
        }
    }

    void ScyllaDBPreparedStatement::recreateStatement()
    {
        SCYLLADB_DEBUG("ScyllaDBPreparedStatement::recreateStatement - Binding prepared statement");
        m_statement.reset(cass_prepared_bind(m_prepared.get()));
    }

    // Public methods

    ScyllaDBPreparedStatement::ScyllaDBPreparedStatement(std::weak_ptr<CassSession> session, const std::string &query, const CassPrepared *prepared)
        : m_session(session), m_query(query), m_prepared(prepared, CassPreparedDeleter()) // Shared ptr to prepared
    {
        SCYLLADB_DEBUG("ScyllaDBPreparedStatement::constructor - Creating prepared statement for query: " << query);
        recreateStatement();
    }

    ScyllaDBPreparedStatement::~ScyllaDBPreparedStatement()
    {
        SCYLLADB_DEBUG("ScyllaDBPreparedStatement::destructor - Destroying prepared statement");
        ScyllaDBPreparedStatement::close(std::nothrow);
    }

    void ScyllaDBPreparedStatement::setInt(int parameterIndex, int value)
    {
        auto result = setInt(std::nothrow, parameterIndex, value);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    void ScyllaDBPreparedStatement::setLong(int parameterIndex, long value)
    {
        auto result = setLong(std::nothrow, parameterIndex, value);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    void ScyllaDBPreparedStatement::setDouble(int parameterIndex, double value)
    {
        auto result = setDouble(std::nothrow, parameterIndex, value);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    void ScyllaDBPreparedStatement::setString(int parameterIndex, const std::string &value)
    {
        auto result = setString(std::nothrow, parameterIndex, value);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    void ScyllaDBPreparedStatement::setBoolean(int parameterIndex, bool value)
    {
        auto result = setBoolean(std::nothrow, parameterIndex, value);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    void ScyllaDBPreparedStatement::setNull(int parameterIndex, Types type)
    {
        auto result = setNull(std::nothrow, parameterIndex, type);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    void ScyllaDBPreparedStatement::setDate(int parameterIndex, const std::string &value)
    {
        auto result = setDate(std::nothrow, parameterIndex, value);
        if (!result.has_value())
            throw result.error();
    }

    void ScyllaDBPreparedStatement::setTimestamp(int parameterIndex, const std::string &value)
    {
        auto result = setTimestamp(std::nothrow, parameterIndex, value);
        if (!result.has_value())
            throw result.error();
    }

    void ScyllaDBPreparedStatement::setUUID(int parameterIndex, const std::string &value)
    {
        auto result = setUUID(std::nothrow, parameterIndex, value);
        if (!result.has_value())
            throw result.error();
    }

    void ScyllaDBPreparedStatement::setBinaryStream(int parameterIndex, std::shared_ptr<InputStream> x)
    {
        auto result = setBinaryStream(std::nothrow, parameterIndex, x);
        if (!result.has_value())
            throw result.error();
    }

    void ScyllaDBPreparedStatement::setBinaryStream(int parameterIndex, std::shared_ptr<InputStream> x, size_t length)
    {
        auto result = setBinaryStream(std::nothrow, parameterIndex, x, length);
        if (!result.has_value())
            throw result.error();
    }

    void ScyllaDBPreparedStatement::setBytes(int parameterIndex, const std::vector<uint8_t> &x)
    {
        auto result = setBytes(std::nothrow, parameterIndex, x);
        if (!result.has_value())
            throw result.error();
    }

    void ScyllaDBPreparedStatement::setBytes(int parameterIndex, const uint8_t *x, size_t length)
    {
        auto result = setBytes(std::nothrow, parameterIndex, x, length);
        if (!result.has_value())
            throw result.error();
    }

    std::shared_ptr<ColumnarDBResultSet> ScyllaDBPreparedStatement::executeQuery()
    {
        auto result = executeQuery(std::nothrow);
        if (!result.has_value())
            throw result.error();
        return *result;
    }

    uint64_t ScyllaDBPreparedStatement::executeUpdate()
    {
        auto result = executeUpdate(std::nothrow);
        if (!result.has_value())
            throw result.error();
        return *result;
    }

    bool ScyllaDBPreparedStatement::execute()
    {
        auto result = execute(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    void ScyllaDBPreparedStatement::addBatch()
    {
        auto result = addBatch(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    void ScyllaDBPreparedStatement::clearBatch()
    {
        auto result = clearBatch(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    std::vector<uint64_t> ScyllaDBPreparedStatement::executeBatch()
    {
        auto result = executeBatch(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    void ScyllaDBPreparedStatement::close()
    {
        auto result = close(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

} // namespace cpp_dbc::ScyllaDB

#endif // USE_SCYLLADB
