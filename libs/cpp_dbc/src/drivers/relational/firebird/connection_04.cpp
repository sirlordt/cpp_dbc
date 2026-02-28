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

 @file connection_04.cpp
 @brief Firebird database driver implementation - FirebirdDBConnection nothrow methods (part 2)

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

    cpp_dbc::expected<void, DBException>
    FirebirdDBConnection::commit(std::nothrow_t) noexcept
    {
        FIREBIRD_DEBUG("FirebirdConnection::commit(nothrow) - Starting");

        FIREBIRD_DEBUG("  Acquiring lock...");
        FIREBIRD_CONNECTION_LOCK_OR_RETURN("WXETVZN5SFDN", "Connection closed");
        FIREBIRD_DEBUG("  Lock acquired");

        FIREBIRD_DEBUG("  Calling endTransaction(true)...");
        auto endResult = endTransaction(std::nothrow, true);
        if (!endResult.has_value())
        {
            return cpp_dbc::unexpected(endResult.error());
        }
        FIREBIRD_DEBUG("  endTransaction completed");

        if (m_autoCommit)
        {
            FIREBIRD_DEBUG("  AutoCommit is enabled, calling startTransaction()...");
            auto startResult = startTransaction(std::nothrow);
            if (!startResult.has_value())
            {
                return cpp_dbc::unexpected(startResult.error());
            }
            FIREBIRD_DEBUG("  startTransaction completed");
        }

        FIREBIRD_DEBUG("FirebirdConnection::commit(nothrow) - Done");
        return {};
    }

    cpp_dbc::expected<void, DBException>
    FirebirdDBConnection::rollback(std::nothrow_t) noexcept
    {
        FIREBIRD_DEBUG("FirebirdConnection::rollback(nothrow) - Starting");

        FIREBIRD_CONNECTION_LOCK_OR_RETURN("FYUWP3CPT23W", "Connection closed");

        FIREBIRD_DEBUG("  Calling endTransaction(false)...");
        auto endResult = endTransaction(std::nothrow, false);
        if (!endResult.has_value())
        {
            return cpp_dbc::unexpected(endResult.error());
        }
        FIREBIRD_DEBUG("  endTransaction completed");

        if (m_autoCommit)
        {
            FIREBIRD_DEBUG("  AutoCommit is enabled, calling startTransaction()...");
            auto startResult = startTransaction(std::nothrow);
            if (!startResult.has_value())
            {
                return cpp_dbc::unexpected(startResult.error());
            }
            FIREBIRD_DEBUG("  startTransaction completed");
        }

        FIREBIRD_DEBUG("FirebirdConnection::rollback(nothrow) - Done");
        return {};
    }

    cpp_dbc::expected<void, cpp_dbc::DBException> FirebirdDBConnection::close(std::nothrow_t) noexcept
    {
        try
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

            FIREBIRD_DEBUG("FirebirdConnection::close - Done");

            // Small delay to ensure cleanup
            std::this_thread::sleep_for(std::chrono::milliseconds(5));

            return {};
        }
        catch (const cpp_dbc::DBException &e)
        {
            FIREBIRD_DEBUG("FirebirdConnection::close - DBException: %s", e.what_s().data());
            return cpp_dbc::unexpected(e);
        }
        catch (const std::exception &e)
        {
            FIREBIRD_DEBUG("FirebirdConnection::close - Exception: %s", e.what());
            return cpp_dbc::unexpected(cpp_dbc::DBException("9DQ74I538ICT",
                                                            std::string("Close failed: ") + e.what(),
                                                            cpp_dbc::system_utils::captureCallStack()));
        }
        catch (...)
        {
            FIREBIRD_DEBUG("FirebirdConnection::close - Unknown exception");
            return cpp_dbc::unexpected(cpp_dbc::DBException("O1ZNL5B5MZQ5",
                                                            "Close failed with unknown error",
                                                            cpp_dbc::system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<void, DBException>
    FirebirdDBConnection::setTransactionIsolation(std::nothrow_t, TransactionIsolationLevel level) noexcept
    {
        try
        {
            FIREBIRD_DEBUG("FirebirdConnection::setTransactionIsolation(nothrow) - Starting");
            FIREBIRD_DEBUG("  Current level: %d", static_cast<int>(m_isolationLevel));
            FIREBIRD_DEBUG("  New level: %d", static_cast<int>(level));

            FIREBIRD_CONNECTION_LOCK_OR_RETURN("67HASSXYPRLT", "Connection closed");

            // If the isolation level is already set to the requested level, do nothing
            if (m_isolationLevel == level)
            {
                FIREBIRD_DEBUG("  No change needed, returning");
                return {};
            }

            // If a transaction is active, we need to end it first, change the isolation level,
            // and restart the transaction with the new isolation level
            bool hadActiveTransaction = m_transactionActive;
            if (m_transactionActive)
            {
                FIREBIRD_DEBUG("  Transaction is active, ending it first");
                // Commit the current transaction (or rollback if autocommit is off)
                if (m_autoCommit)
                {
                    FIREBIRD_DEBUG("  AutoCommit is enabled, committing current transaction");
                    auto endResult = endTransaction(std::nothrow, true);
                    if (!endResult.has_value())
                    {
                        return cpp_dbc::unexpected(endResult.error());
                    }
                }
                else
                {
                    FIREBIRD_DEBUG("  AutoCommit is disabled, rolling back current transaction");
                    auto endResult = endTransaction(std::nothrow, false);
                    if (!endResult.has_value())
                    {
                        return cpp_dbc::unexpected(endResult.error());
                    }
                }
            }

            m_isolationLevel = level;
            FIREBIRD_DEBUG("  Isolation level set to: %d", static_cast<int>(m_isolationLevel));

            // Restart the transaction if we had one active and autocommit is on
            if (hadActiveTransaction && m_autoCommit)
            {
                FIREBIRD_DEBUG("  Restarting transaction with new isolation level");
                auto startResult = startTransaction(std::nothrow);
                if (!startResult.has_value())
                {
                    FIREBIRD_DEBUG("  Failed to restart transaction: %s", startResult.error().what_s().data());
                    return cpp_dbc::unexpected(startResult.error());
                }
            }

            FIREBIRD_DEBUG("FirebirdConnection::setTransactionIsolation(nothrow) - Done");
            return {};
        }
        catch (const DBException &e)
        {
            return cpp_dbc::unexpected(e);
        }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("E7F8A9B0C1D2", std::string("Exception in setTransactionIsolation: ") + e.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("F8A9B0C1D2E3", "Unknown exception in setTransactionIsolation",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<TransactionIsolationLevel, DBException>
    FirebirdDBConnection::getTransactionIsolation(std::nothrow_t) noexcept
    {
        try
        {
            FIREBIRD_CONNECTION_LOCK_OR_RETURN("W27RDYMQ9TXH", "Connection closed");

            FIREBIRD_DEBUG("FirebirdConnection::getTransactionIsolation(nothrow) - Returning %d", static_cast<int>(m_isolationLevel));
            return m_isolationLevel;
        }
        catch (const DBException &e)
        {
            return cpp_dbc::unexpected(e);
        }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("G9A0B1C2D3E4", std::string("Exception in getTransactionIsolation: ") + e.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("H0B1C2D3E4F5", "Unknown exception in getTransactionIsolation",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<bool, DBException>
    FirebirdDBConnection::isClosed(std::nothrow_t) const noexcept
    {
        return m_closed.load(std::memory_order_acquire);
    }

    cpp_dbc::expected<void, DBException>
    FirebirdDBConnection::returnToPool(std::nothrow_t) noexcept
    {
        try
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
        catch (const DBException &e)
        {
            return cpp_dbc::unexpected(e);
        }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("21I433I3JGSM",
                                                   std::string("returnToPool failed: ") + e.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("RX0KXBFWZ9AP",
                                                   "returnToPool failed with unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<bool, DBException>
    FirebirdDBConnection::isPooled(std::nothrow_t) const noexcept
    {
        return false;
    }

    cpp_dbc::expected<std::string, DBException>
    FirebirdDBConnection::getURL(std::nothrow_t) const noexcept
    {
        return m_url;
    }

    cpp_dbc::expected<void, DBException>
    FirebirdDBConnection::prepareForPoolReturn(std::nothrow_t) noexcept
    {
        // Reset connection state: close all statements/resultsets and rollback
        auto resetResult = reset(std::nothrow);
        if (!resetResult.has_value())
        {
            FIREBIRD_DEBUG("prepareForPoolReturn(nothrow): Failed to reset: %s",
                           resetResult.error().what_s().data());
            // Continue anyway - try to at least reset autoCommit
        }

        // Reset auto-commit to true (default state for pooled connections)
        setAutoCommit(std::nothrow, true);

        return {};
    }

    cpp_dbc::expected<void, DBException>
    FirebirdDBConnection::prepareForBorrow(std::nothrow_t) noexcept
    {
        // See MVCC documentation in connection_01.cpp prepareForBorrow() comment block
        // for explanation of why this is needed.
        FIREBIRD_CONNECTION_LOCK_OR_RETURN("HR0QTJ2GVXFT", "Connection closed");

        if (m_closed.load(std::memory_order_acquire))
        {
            return {};
        }

        // Refresh the MVCC snapshot by rolling back and starting a new transaction.
        // This ensures the borrowed connection can see all DDL changes committed
        // after the previous transaction started.
        if (m_tr && m_autoCommit)
        {
            FIREBIRD_DEBUG("FirebirdConnection::prepareForBorrow(nothrow) - Refreshing MVCC snapshot");
            [[maybe_unused]] auto rollbackResult = rollback(std::nothrow);
        }

        return {};
    }

} // namespace cpp_dbc::Firebird

#endif // USE_FIREBIRD
