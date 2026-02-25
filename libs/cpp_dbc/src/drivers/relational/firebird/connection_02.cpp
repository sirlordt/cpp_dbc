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

 * This file is part of the cpp_dbc project and is licensed under the GNU GPL v3.
 * See the LICENSE.md file in the project root for more information.

 @file connection_02.cpp
 @brief Firebird database driver implementation - FirebirdDBConnection throwing methods (part 2)

*/

#include "cpp_dbc/drivers/relational/driver_firebird.hpp"

#if USE_FIREBIRD

#include <sstream>
#include <algorithm>
#include <chrono>
#include <thread>
#include <cstring>
#include <cstdlib>
#include <cmath>

#include "firebird_internal.hpp"

namespace cpp_dbc::Firebird
{

    void FirebirdDBConnection::close()
    {
        auto result = close(std::nothrow);
        if (!result)
        {
            throw result.error();
        }
    }

    void FirebirdDBConnection::reset()
    {
        auto result = reset(std::nothrow);
        if (!result)
        {
            throw result.error();
        }
    }

    bool FirebirdDBConnection::isClosed() const
    {
        auto result = isClosed(std::nothrow);
        if (!result)
        {
            throw result.error();
        }
        return result.value();
    }

    void FirebirdDBConnection::returnToPool()
    {
        auto result = returnToPool(std::nothrow);
        if (!result)
        {
            throw result.error();
        }
    }

    bool FirebirdDBConnection::isPooled() const
    {
        auto result = isPooled(std::nothrow);
        if (!result)
        {
            throw result.error();
        }
        return result.value();
    }

    std::shared_ptr<RelationalDBPreparedStatement> FirebirdDBConnection::prepareStatement(const std::string &sql)
    {
        auto result = prepareStatement(std::nothrow, sql);
        if (!result)
        {
            throw result.error();
        }
        return result.value();
    }

    std::shared_ptr<RelationalDBResultSet> FirebirdDBConnection::executeQuery(const std::string &sql)
    {
        auto result = executeQuery(std::nothrow, sql);
        if (!result)
        {
            throw result.error();
        }
        return result.value();
    }

    uint64_t FirebirdDBConnection::executeUpdate(const std::string &sql)
    {
        auto result = executeUpdate(std::nothrow, sql);
        if (!result)
        {
            throw result.error();
        }
        return result.value();
    }

    void FirebirdDBConnection::setAutoCommit(bool autoCommit)
    {
        auto result = setAutoCommit(std::nothrow, autoCommit);
        if (!result)
        {
            throw result.error();
        }
    }

    bool FirebirdDBConnection::getAutoCommit()
    {
        auto result = getAutoCommit(std::nothrow);
        if (!result)
        {
            throw result.error();
        }
        return *result;
    }

    bool FirebirdDBConnection::beginTransaction()
    {
        auto result = beginTransaction(std::nothrow);
        if (!result)
        {
            throw result.error();
        }
        return *result;
    }

    bool FirebirdDBConnection::transactionActive()
    {
        auto result = transactionActive(std::nothrow);
        if (!result)
        {
            throw result.error();
        }
        return *result;
    }

    void FirebirdDBConnection::commit()
    {
        auto result = commit(std::nothrow);
        if (!result)
        {
            throw result.error();
        }
    }

    void FirebirdDBConnection::rollback()
    {
        auto result = rollback(std::nothrow);
        if (!result)
        {
            throw result.error();
        }
    }

    void FirebirdDBConnection::prepareForPoolReturn()
    {
        auto result = prepareForPoolReturn(std::nothrow);
        if (!result)
        {
            throw result.error();
        }
    }

    // ============================================================================
    // MVCC (Multi-Version Concurrency Control) Fix for Connection Pooling
    // ============================================================================
    //
    // PROBLEM:
    // Firebird uses MVCC, where each transaction sees a "snapshot" of the database
    // taken at the moment the transaction starts. This snapshot is immutable for
    // the lifetime of the transaction.
    //
    // In a connection pool scenario:
    // 1. Pool creates N connections at initialization time (e.g., T=0)
    // 2. Each connection starts a transaction immediately (auto-commit mode)
    // 3. These transactions capture a snapshot of the database at T=0
    // 4. Later (e.g., T=10), a test runs RECREATE TABLE or other DDL
    // 5. When a pooled connection is borrowed, its transaction still has the T=0 snapshot
    // 6. The T=0 snapshot doesn't "see" the table created at T=10
    // 7. Result: "SQLCODE -219: table id not defined" errors
    //
    // SYMPTOMS:
    // - Intermittent "table id not defined" errors in pool tests
    // - First connection works (gets fresh transaction), subsequent ones fail
    // - Errors appear after DDL operations (CREATE, DROP, ALTER, RECREATE TABLE)
    // - Pool timeout errors may also appear as a secondary effect
    //
    // SOLUTION:
    // When borrowing a connection from the pool, we rollback the current transaction
    // and start a new one. This forces Firebird to take a fresh MVCC snapshot that
    // includes all committed DDL changes up to that moment.
    //
    // ============================================================================
    // WARNING: COMMON MISDIAGNOSIS - READ THIS BEFORE MODIFYING
    // ============================================================================
    //
    // When debugging pool issues with symptoms like "timeout" + "table id not defined",
    // it may seem logical to blame closeAllActivePreparedStatements() in
    // prepareForPoolReturn(), thinking:
    //
    //   "Statement closing is slow under Valgrind → causes timeouts → connections
    //    with stale transactions get borrowed → table errors"
    //
    // THIS IS A FALSE DIAGNOSIS. The actual cause is the MVCC snapshot issue described
    // above. The statement closing and MVCC are COMPLETELY UNRELATED concerns:
    //
    // +---------------------------+----------------------------------------+
    // | Statement Closing         | MVCC Refresh                           |
    // +---------------------------+----------------------------------------+
    // | Resource management       | Database visibility                    |
    // | Releases handles/memory   | Determines what tables/data are seen   |
    // | Done in prepareForReturn  | Done in prepareForBorrow               |
    // | Can be slow under Valgrind| Always fast (just rollback+start tx)   |
    // | Does NOT cause MVCC issues| FIXES the MVCC visibility problem      |
    // +---------------------------+----------------------------------------+
    //
    // If you see "table id not defined" errors in pool tests, the fix is HERE in
    // prepareForBorrow(), NOT in removing statement closing from prepareForPoolReturn().
    //
    // This was verified experimentally: with prepareForBorrow() active, both
    // active statement closing AND passive clearing work correctly. The MVCC
    // refresh is the essential fix.
    //
    // ============================================================================
    void FirebirdDBConnection::prepareForBorrow()
    {
        auto result = prepareForBorrow(std::nothrow);
        if (!result)
        {
            throw result.error();
        }
    }

    void FirebirdDBConnection::setTransactionIsolation(TransactionIsolationLevel level)
    {
        auto result = setTransactionIsolation(std::nothrow, level);
        if (!result)
        {
            throw result.error();
        }
    }

    TransactionIsolationLevel FirebirdDBConnection::getTransactionIsolation()
    {
        auto result = getTransactionIsolation(std::nothrow);
        if (!result)
        {
            throw result.error();
        }
        return *result;
    }

    std::string FirebirdDBConnection::getURL() const
    {
        auto result = getURL(std::nothrow);
        if (!result)
        {
            throw result.error();
        }
        return *result;
    }

} // namespace cpp_dbc::Firebird

#endif // USE_FIREBIRD
