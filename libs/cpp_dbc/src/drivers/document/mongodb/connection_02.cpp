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
 * @file connection_02.cpp
 * @brief MongoDB MongoDBConnection - Part 2 (collection operations, commands, sessions, transactions)
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

    std::shared_ptr<DocumentDBCollection> MongoDBConnection::getCollection(const std::string &collectionName)
    {
        auto result = getCollection(std::nothrow, collectionName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    std::vector<std::string> MongoDBConnection::listCollections()
    {
        auto result = listCollections(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    bool MongoDBConnection::collectionExists(const std::string &collectionName)
    {
        auto result = collectionExists(std::nothrow, collectionName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    std::shared_ptr<DocumentDBCollection> MongoDBConnection::createCollection(
        const std::string &collectionName,
        const std::string &options)
    {
        auto result = createCollection(std::nothrow, collectionName, options);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    void MongoDBConnection::dropCollection(const std::string &collectionName)
    {
        auto result = dropCollection(std::nothrow, collectionName);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    std::shared_ptr<DocumentDBData> MongoDBConnection::createDocument()
    {
        auto result = createDocument(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    std::shared_ptr<DocumentDBData> MongoDBConnection::createDocument(const std::string &json)
    {
        auto result = createDocument(std::nothrow, json);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    std::shared_ptr<DocumentDBData> MongoDBConnection::runCommand(const std::string &command)
    {
        auto result = runCommand(std::nothrow, command);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    std::shared_ptr<DocumentDBData> MongoDBConnection::getServerInfo()
    {
        auto result = getServerInfo(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    std::shared_ptr<DocumentDBData> MongoDBConnection::getServerStatus()
    {
        auto result = getServerStatus(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    bool MongoDBConnection::ping()
    {
        auto result = ping(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    std::string MongoDBConnection::startSession()
    {
        auto result = startSession(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    void MongoDBConnection::endSession(const std::string &sessionId)
    {
        auto result = endSession(std::nothrow, sessionId);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    void MongoDBConnection::startTransaction(const std::string &sessionId)
    {
        auto result = startTransaction(std::nothrow, sessionId);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    void MongoDBConnection::commitTransaction(const std::string &sessionId)
    {
        auto result = commitTransaction(std::nothrow, sessionId);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    void MongoDBConnection::abortTransaction(const std::string &sessionId)
    {
        auto result = abortTransaction(std::nothrow, sessionId);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    bool MongoDBConnection::supportsTransactions()
    {
        auto result = supportsTransactions(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    void MongoDBConnection::prepareForPoolReturn()
    {
        auto result = prepareForPoolReturn(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    void MongoDBConnection::prepareForBorrow()
    {
        auto result = prepareForBorrow(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

} // namespace cpp_dbc::MongoDB

#endif // USE_MONGODB
