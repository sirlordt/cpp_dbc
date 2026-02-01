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
 * @file connection_04.cpp
 * @brief MongoDB MongoDBConnection - Part 4 (nothrow versions: document and command operations)
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
    // MongoDBConnection NOTHROW VERSIONS - Document and Command Operations
    // ====================================================================

    expected<std::shared_ptr<DocumentDBData>, DBException> MongoDBConnection::createDocument(std::nothrow_t) noexcept
    {
        try
        {
            auto doc = std::make_shared<MongoDBDocument>();
            return std::static_pointer_cast<DocumentDBData>(doc);
        }
        catch (const DBException &ex)
        {
            return unexpected<DBException>(ex);
        }
        catch ([[maybe_unused]] const std::bad_alloc &ex)
        {
            return unexpected<DBException>(DBException(
                "0B1C2D3E4F5A",
                "Memory allocation failed in createDocument"));
        }
        catch (const std::exception &ex)
        {
            return unexpected<DBException>(DBException(
                "1C2D3E4F5A6B",
                std::string("Unexpected error in createDocument: ") + ex.what()));
        }
        catch (...)
        {
            return unexpected<DBException>(DBException(
                "2D3E4F5A6B7C",
                "Unknown error in createDocument"));
        }
    }

    expected<std::shared_ptr<DocumentDBData>, DBException> MongoDBConnection::createDocument(
        std::nothrow_t, const std::string &json) noexcept
    {
        try
        {
            auto doc = std::make_shared<MongoDBDocument>(json);
            return std::static_pointer_cast<DocumentDBData>(doc);
        }
        catch (const DBException &ex)
        {
            return unexpected<DBException>(ex);
        }
        catch ([[maybe_unused]] const std::bad_alloc &ex)
        {
            return unexpected<DBException>(DBException(
                "3E4F5A6B7C8D",
                "Memory allocation failed in createDocument"));
        }
        catch (const std::exception &ex)
        {
            return unexpected<DBException>(DBException(
                "4F5A6B7C8D9E",
                std::string("Failed to parse JSON in createDocument: ") + ex.what()));
        }
        catch (...)
        {
            return unexpected<DBException>(DBException(
                "5A6B7C8D9E0F",
                "Unknown error in createDocument"));
        }
    }

    expected<std::shared_ptr<DocumentDBData>, DBException> MongoDBConnection::runCommand(
        std::nothrow_t, const std::string &command) noexcept
    {
        try
        {
            MONGODB_LOCK_GUARD(m_connMutex);

            if (m_closed.load())
            {
                return unexpected<DBException>(DBException(
                    "6B7C8D9E0F1A",
                    "Connection has been closed"));
            }

            if (m_databaseName.empty())
            {
                return unexpected<DBException>(DBException(
                    "7C8D9E0F1A2B",
                    "No database selected. Call useDatabase() first"));
            }

            BsonHandle cmdBson = makeBsonHandleFromJson(command);

            MongoDatabaseHandle db(mongoc_client_get_database(m_client.get(), m_databaseName.c_str()));

            bson_error_t error;
            bson_t reply;
            bson_init(&reply);

            bool success = mongoc_database_command_simple(db.get(), cmdBson.get(), nullptr, &reply, &error);

            if (!success)
            {
                bson_destroy(&reply);
                return unexpected<DBException>(DBException(
                    "8D9E0F1A2B3C",
                    std::string("Command failed: ") + error.message));
            }

            bson_t *replyCopy = bson_copy(&reply);
            bson_destroy(&reply);

            if (!replyCopy)
            {
                return unexpected<DBException>(DBException(
                    "9E0F1A2B3C4D",
                    "Failed to copy command reply"));
            }

            auto doc = std::make_shared<MongoDBDocument>(replyCopy);
            return std::static_pointer_cast<DocumentDBData>(doc);
        }
        catch (const DBException &ex)
        {
            return unexpected<DBException>(ex);
        }
        catch ([[maybe_unused]] const std::bad_alloc &ex)
        {
            return unexpected<DBException>(DBException(
                "0F1A2B3C4D5E",
                "Memory allocation failed in runCommand"));
        }
        catch (const std::exception &ex)
        {
            return unexpected<DBException>(DBException(
                "1A2B3C4D5E6F",
                std::string("Unexpected error in runCommand: ") + ex.what()));
        }
        catch (...)
        {
            return unexpected<DBException>(DBException(
                "2B3C4D5E6F7A",
                "Unknown error in runCommand"));
        }
    }

    expected<std::shared_ptr<DocumentDBData>, DBException> MongoDBConnection::getServerInfo(std::nothrow_t) noexcept
    {
        try
        {
            return runCommand(std::nothrow, "{\"buildInfo\": 1}");
        }
        catch (const DBException &ex)
        {
            return unexpected<DBException>(ex);
        }
        catch (const std::exception &ex)
        {
            return unexpected<DBException>(DBException(
                "3C4D5E6F7A8B",
                std::string("Error in getServerInfo: ") + ex.what()));
        }
        catch (...)
        {
            return unexpected<DBException>(DBException(
                "4D5E6F7A8B9C",
                "Unknown error in getServerInfo"));
        }
    }

    expected<std::shared_ptr<DocumentDBData>, DBException> MongoDBConnection::getServerStatus(std::nothrow_t) noexcept
    {
        try
        {
            return runCommand(std::nothrow, "{\"serverStatus\": 1}");
        }
        catch (const DBException &ex)
        {
            return unexpected<DBException>(ex);
        }
        catch (const std::exception &ex)
        {
            return unexpected<DBException>(DBException(
                "5E6F7A8B9C0D",
                std::string("Error in getServerStatus: ") + ex.what()));
        }
        catch (...)
        {
            return unexpected<DBException>(DBException(
                "6F7A8B9C0D1E",
                "Unknown error in getServerStatus"));
        }
    }

    // ====================================================================
    // MongoDBConnection - MongoDB-specific public methods
    // ====================================================================

    std::weak_ptr<mongoc_client_t> MongoDBConnection::getClientWeak() const
    {
        MONGODB_LOCK_GUARD(m_connMutex);
        return std::weak_ptr<mongoc_client_t>(m_client);
    }

    MongoClientHandle MongoDBConnection::getClient() const
    {
        MONGODB_LOCK_GUARD(m_connMutex);
        return m_client;
    }

    void MongoDBConnection::setPooled(bool pooled)
    {
        MONGODB_LOCK_GUARD(m_connMutex);
        m_pooled = pooled;
    }

} // namespace cpp_dbc::MongoDB

#endif // USE_MONGODB
