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
 * @file collection_07.cpp
 * @brief MongoDB MongoDBCollection - Part 7 (throwing versions: INDEX + COLLECTION ops + isConnectionValid)
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
    // INDEX OPERATIONS - throwing versions (WRAPPERS)
    // ====================================================================

    std::string MongoDBCollection::createIndex(const std::string &keys, const std::string &options)
    {
        auto result = createIndex(std::nothrow, keys, options);
        if (!result)
        {
            throw result.error();
        }
        return *result;
    }

    void MongoDBCollection::dropIndex(const std::string &indexName)
    {
        auto result = dropIndex(std::nothrow, indexName);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    void MongoDBCollection::dropAllIndexes()
    {
        auto result = dropAllIndexes(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    std::vector<std::string> MongoDBCollection::listIndexes()
    {
        auto result = listIndexes(std::nothrow);
        if (!result)
        {
            throw result.error();
        }
        return *result;
    }

    // ====================================================================
    // COLLECTION OPERATIONS - throwing versions (WRAPPERS)
    // ====================================================================

    void MongoDBCollection::drop()
    {
        auto result = drop(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    void MongoDBCollection::rename(const std::string &newName, bool dropTarget)
    {
        auto result = rename(std::nothrow, newName, dropTarget);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    std::shared_ptr<DocumentDBCursor> MongoDBCollection::aggregate(const std::string &pipeline)
    {
        auto result = aggregate(std::nothrow, pipeline);
        if (!result)
        {
            throw result.error();
        }
        return *result;
    }

    std::vector<std::string> MongoDBCollection::distinct(
        const std::string &fieldPath,
        const std::string &filter)
    {
        auto result = distinct(std::nothrow, fieldPath, filter);
        if (!result)
        {
            throw result.error();
        }
        return *result;
    }

    // ====================================================================
    // CONNECTION VALIDATION
    // ====================================================================

    bool MongoDBCollection::isConnectionValid() const
    {
        return !m_client.expired();
    }

} // namespace cpp_dbc::MongoDB

#endif // USE_MONGODB
