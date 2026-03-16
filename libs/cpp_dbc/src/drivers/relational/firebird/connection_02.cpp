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
 @brief Firebird database driver implementation - FirebirdDBConnection nothrow methods (group 1: close, reset, isClosed, returnToPool, isPooled, getURI, ping)

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

    // ============================================================================
    // NOTHROW API — group 1: close, reset, isClosed, returnToPool, isPooled, getURI, ping
    // ============================================================================

    cpp_dbc::expected<void, cpp_dbc::DBException> FirebirdDBConnection::close(std::nothrow_t) noexcept
    {
        FIREBIRD_CONNECTION_LOCK_OR_RETURN_SUCCESS_IF_CLOSED();

        FIREBIRD_DEBUG("FirebirdConnection::close - Starting");

        // Reset connection state (close statements/resultsets, rollback)
        // Note: reset() already acquired the lock, but it's recursive so it's safe
        auto resetResult = reset(std::nothrow);
        if (!resetResult.has_value())
        {
            FIREBIRD_DEBUG("  Failed to reset connection state: %s", resetResult.error().what_s().data());
            // Continue with close even if reset failed
        }

        // The database handle will be closed by the shared_ptr deleter
        m_db.reset();

        m_closed.store(true, std::memory_order_release);

        // Unregister from the driver registry so getConnectionAlive() reflects
        // actual live connections. The owner_less m_self weak_ptr is used for
        // set lookup — raw 'this' would not match the set's comparator.
        FirebirdDBDriver::unregisterConnection(std::nothrow, m_self);

        FIREBIRD_DEBUG("FirebirdConnection::close - Done");

        // Small delay to ensure cleanup
        std::this_thread::sleep_for(std::chrono::milliseconds(5));

        return {};
    }

    cpp_dbc::expected<void, cpp_dbc::DBException> FirebirdDBConnection::reset(std::nothrow_t) noexcept
    {
        FIREBIRD_DEBUG("FirebirdConnection::reset - Starting");

        FIREBIRD_CONNECTION_LOCK_OR_RETURN("XLGNQG433BAJ", "Connection closed");

        // RAII guard to set m_resetting flag during reset operation
        // This prevents ResultSet/PreparedStatement from calling unregister during closeAll*()
        cpp_dbc::system_utils::AtomicGuard<bool> resettingGuard(m_resetting, true, false);

        // Close all active prepared statements first (releases metadata locks)
        auto closeStmtsResult = closeAllStatements(std::nothrow);
        if (!closeStmtsResult.has_value())
        {
            FIREBIRD_DEBUG("  reset: closeAllStatements failed: %s", closeStmtsResult.error().what_s().data());
        }

        // Close all active result sets (releases cursor resources)
        auto closeRsResult = closeAllResultSets(std::nothrow);
        if (!closeRsResult.has_value())
        {
            FIREBIRD_DEBUG("  reset: closeAllResultSets failed: %s", closeRsResult.error().what_s().data());
        }

        // Rollback any active transaction to clean state
        if (m_tr)
        {
            FIREBIRD_DEBUG("  Rolling back active transaction");
            ISC_STATUS_ARRAY status;
            isc_rollback_transaction(status, &m_tr);
            m_tr = 0;
            m_transactionActive = false;
        }

        // Restore autoCommit to true so the next borrower gets a clean state.
        // beginTransaction() sets m_autoCommit=false, and if the user returns
        // the connection without committing/rolling back, this ensures the flag
        // is reset. Consistent with SQLite's reset() behaviour.
        m_autoCommit = true;

        FIREBIRD_DEBUG("FirebirdConnection::reset - Done");
        return {};
    }

    cpp_dbc::expected<bool, DBException>
    FirebirdDBConnection::isClosed(std::nothrow_t) const noexcept
    {
        return m_closed.load(std::memory_order_acquire);
    }

    cpp_dbc::expected<void, DBException>
    FirebirdDBConnection::returnToPool(std::nothrow_t) noexcept
    {
        FIREBIRD_CONNECTION_LOCK_OR_RETURN("HFEEQDUN8QKD", "Connection closed");

        FIREBIRD_DEBUG("FirebirdConnection::returnToPool(nothrow) - Starting");

        // Prepare for pool return: close all statements/resultsets, rollback, reset autocommit.
        // prepareForPoolReturn always returns success internally (failures are non-fatal and logged
        // inside), but check the result to satisfy [[nodiscard]] and make intent explicit.
        auto prepResult = prepareForPoolReturn(std::nothrow);
        if (!prepResult.has_value())
        {
            FIREBIRD_DEBUG("  prepareForPoolReturn failed: %s", prepResult.error().what_s().data());
        }

        // Start a fresh transaction for the next use
        if (!m_tr && !m_closed.load(std::memory_order_acquire))
        {
            FIREBIRD_DEBUG("  Starting fresh transaction for pool reuse");
            [[maybe_unused]] auto startResult = startTransaction(std::nothrow);
            if (!startResult.has_value())
            {
                FIREBIRD_DEBUG("  Failed to start fresh transaction");
            }
        }

        FIREBIRD_DEBUG("FirebirdConnection::returnToPool(nothrow) - Done, m_tr=%ld", (long)m_tr);
        return {};
    }

    cpp_dbc::expected<bool, DBException>
    FirebirdDBConnection::isPooled(std::nothrow_t) const noexcept
    {
        return false;
    }

    // No try/catch: the only possible throw is std::bad_alloc from the
    // std::string copy, which is a death-sentence exception — no meaningful
    // recovery is possible, so std::terminate is the correct response.
    cpp_dbc::expected<std::string, DBException>
    FirebirdDBConnection::getURI(std::nothrow_t) const noexcept
    {
        return m_uri;
    }

    cpp_dbc::expected<bool, DBException> FirebirdDBConnection::ping(std::nothrow_t) noexcept
    {
        auto result = executeQuery(std::nothrow, "SELECT 1 FROM RDB$DATABASE");
        if (!result.has_value())
        {
            return cpp_dbc::unexpected(result.error());
        }
        auto closeResult = result.value()->close(std::nothrow);
        if (!closeResult.has_value())
        {
            return cpp_dbc::unexpected(closeResult.error());
        }
        return true;
    }

} // namespace cpp_dbc::Firebird

#endif // USE_FIREBIRD
