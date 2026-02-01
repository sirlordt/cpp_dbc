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
 * @file document_04.cpp
 * @brief MongoDB MongoDBDocument - Part 4 (field operations, utilities)
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

    bool MongoDBDocument::hasField(const std::string &fieldPath) const
    {
        MONGODB_LOCK_GUARD(m_mutex);

        if (!m_bson)
        {
            return false;
        }

        bson_iter_t iter;
        return navigateToField(fieldPath, iter);
    }

    bool MongoDBDocument::isNull(const std::string &fieldPath) const
    {
        MONGODB_LOCK_GUARD(m_mutex);

        bson_iter_t iter;
        if (!navigateToField(fieldPath, iter))
        {
            return true; // Field doesn't exist, treat as null
        }

        return BSON_ITER_HOLDS_NULL(&iter);
    }

    bool MongoDBDocument::removeField(const std::string &fieldPath)
    {
        MONGODB_LOCK_GUARD(m_mutex);
        validateDocument();

        if (fieldPath.contains('.'))
        {
            throw DBException("6C8902B6F059", "Nested field removal not yet implemented: " + fieldPath, system_utils::captureCallStack());
        }

        // Check if field exists
        bson_iter_t iter;
        if (!navigateToField(fieldPath, iter))
        {
            return false;
        }

        // Create new document without the field
        bson_t *newBson = bson_new();
        if (!newBson)
        {
            throw DBException("957D5DE180B6", "Failed to create new BSON document", system_utils::captureCallStack());
        }

        if (bson_iter_init(&iter, m_bson.get()))
        {
            while (bson_iter_next(&iter))
            {
                const char *key = bson_iter_key(&iter);
                if (fieldPath != key)
                {
                    bson_append_iter(newBson, key, -1, &iter);
                }
            }
        }

        m_bson.reset(newBson);
        m_idCached = false;
        return true;
    }

    std::vector<std::string> MongoDBDocument::getFieldNames() const
    {
        MONGODB_LOCK_GUARD(m_mutex);
        validateDocument();

        std::vector<std::string> names;

        bson_iter_t iter;
        if (bson_iter_init(&iter, m_bson.get()))
        {
            while (bson_iter_next(&iter))
            {
                names.emplace_back(bson_iter_key(&iter));
            }
        }

        return names;
    }

    std::shared_ptr<DocumentDBData> MongoDBDocument::clone() const
    {
        MONGODB_LOCK_GUARD(m_mutex);
        validateDocument();

        bson_t *copy = bson_copy(m_bson.get());
        if (!copy)
        {
            throw DBException("5380CBC18BA5", "Failed to clone document", system_utils::captureCallStack());
        }

        return std::make_shared<MongoDBDocument>(copy);
    }

    void MongoDBDocument::clear()
    {
        MONGODB_LOCK_GUARD(m_mutex);

        bson_t *empty = bson_new();
        if (!empty)
        {
            throw DBException("1672D32248D8", "Failed to create empty BSON document", system_utils::captureCallStack());
        }
        m_bson.reset(empty);

        m_idCached = false;
        m_cachedId.clear();
    }

    bool MongoDBDocument::isEmpty() const
    {
        MONGODB_LOCK_GUARD(m_mutex);

        if (!m_bson)
        {
            return true;
        }

        return bson_count_keys(m_bson.get()) == 0;
    }

    const bson_t *MongoDBDocument::getBson() const
    {
        MONGODB_LOCK_GUARD(m_mutex);
        return m_bson.get();
    }

    bson_t *MongoDBDocument::getBsonMutable()
    {
        MONGODB_LOCK_GUARD(m_mutex);
        m_idCached = false;
        return m_bson.get();
    }

    std::shared_ptr<MongoDBDocument> MongoDBDocument::fromBson(bson_t *bson)
    {
        return std::make_shared<MongoDBDocument>(bson);
    }

    std::shared_ptr<MongoDBDocument> MongoDBDocument::fromBsonCopy(const bson_t *bson)
    {
        if (!bson)
        {
            throw DBException("A7B3C2D1E4F5", "Cannot create document from null BSON pointer", system_utils::captureCallStack());
        }

        bson_t *copy = bson_copy(bson);
        if (!copy)
        {
            throw DBException("B8C4D3E2F5A6", "Failed to copy BSON document", system_utils::captureCallStack());
        }

        return std::make_shared<MongoDBDocument>(copy);
    }

} // namespace cpp_dbc::MongoDB

#endif // USE_MONGODB
