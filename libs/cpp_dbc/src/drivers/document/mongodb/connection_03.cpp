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
 * @file connection_03.cpp
 * @brief MongoDB MongoDBConnection - Part 3 (nothrow API: close, reset, isClosed, returnToPool, isPooled, getURI)
 */

#include "cpp_dbc/drivers/document/driver_mongodb.hpp"

#if USE_MONGODB

#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <ranges>
#include <sstream>
#include <stdexcept>
#include "cpp_dbc/common/system_utils.hpp"
#include "mongodb_internal.hpp"

namespace cpp_dbc::MongoDB
{

    // ====================================================================
    // NOTHROW API - close, reset, isClosed, returnToPool, isPooled, getURI (real implementations)
    // ====================================================================

    expected<void, DBException> MongoDBConnection::close(std::nothrow_t) noexcept
    {
        try
        {
            MONGODB_LOCK_GUARD(*m_connMutex);

            if (m_closed.load(std::memory_order_seq_cst))
            {
                return {};
            }

            MONGODB_DEBUG("MongoDBConnection::close(nothrow) - Closing connection");

            // Close all active cursors BEFORE destroying the client
            MONGODB_DEBUG("MongoDBConnection::close(nothrow) - Closing %zu active cursors", m_activeCursors.size());
            for (const auto &weakCursor : m_activeCursors)
            {
                if (auto cursor = weakCursor.lock())
                {
                    auto r = cursor->close(std::nothrow);
                    if (!r.has_value())
                    {
                        MONGODB_DEBUG("MongoDBConnection::close(nothrow) - Error ignored during cursor cleanup: %s", r.error().what());
                    }
                }
            }
            m_activeCursors.clear();

            // End all active sessions
            m_sessions.clear();

            // Clear active collections
            m_activeCollections.clear();

            m_client.reset();
            m_closed.store(true, std::memory_order_seq_cst);

            // Unregister from the driver registry so getConnectionAlive() reflects
            // actual live connections. The owner_less m_self weak_ptr is used for
            // set lookup — raw 'this' would not match the set's comparator.
            MongoDBDriver::unregisterConnection(std::nothrow, m_self);

            MONGODB_DEBUG("MongoDBConnection::close(nothrow) - Connection closed");
            return {};
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("7DGLQ7C4QX4R",
                                                   std::string("Exception in close: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...) // NOSONAR(cpp:S2738) — fallback for non-std exceptions after typed catch above
        {
            return cpp_dbc::unexpected(DBException("VS3RNHCXXBMC",
                                                   "Unknown exception in close",
                                                   system_utils::captureCallStack()));
        }
    }

    expected<void, DBException> MongoDBConnection::reset(std::nothrow_t) noexcept
    {
        return prepareForPoolReturn(std::nothrow);
    }

    expected<bool, DBException> MongoDBConnection::isClosed(std::nothrow_t) const noexcept
    {
        return m_closed.load(std::memory_order_seq_cst);
    }

    expected<void, DBException> MongoDBConnection::returnToPool(std::nothrow_t) noexcept
    {
        if (m_pooled)
        {
            return reset(std::nothrow);
        }
        return close(std::nothrow);
    }

    expected<bool, DBException> MongoDBConnection::isPooled(std::nothrow_t) const noexcept
    {
        return m_pooled;
    }

    // No try/catch: the only possible throw is std::bad_alloc from the
    // std::string copy, which is a death-sentence exception — no meaningful
    // recovery is possible, so std::terminate is the correct response.
    expected<std::string, DBException> MongoDBConnection::getURI(std::nothrow_t) const noexcept
    {
        return m_uri;
    }

} // namespace cpp_dbc::MongoDB

#endif // USE_MONGODB
