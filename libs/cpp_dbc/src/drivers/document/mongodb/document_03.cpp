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
 * @file document_03.cpp
 * @brief MongoDB MongoDBDocument - Part 3 (getStringArray, setters)
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

    std::vector<std::string> MongoDBDocument::getStringArray(const std::string &fieldPath) const
    {
        MONGODB_LOCK_GUARD(m_mutex);

        bson_iter_t iter;
        if (!navigateToField(fieldPath, iter))
        {
            throw DBException("ABEF081E08DE", "Field not found: " + fieldPath, system_utils::captureCallStack());
        }

        if (!BSON_ITER_HOLDS_ARRAY(&iter))
        {
            throw DBException("K0WR8CEGWEWI", "Field is not an array: " + fieldPath, system_utils::captureCallStack());
        }

        std::vector<std::string> result;

        bson_iter_t arrayIter;
        const uint8_t *data = nullptr;
        uint32_t length = 0;
        bson_iter_array(&iter, &length, &data);

        bson_t arrayBson;
        if (!bson_init_static(&arrayBson, data, length))
        {
            throw DBException("F75CDC822CB1", "Failed to initialize array BSON", system_utils::captureCallStack());
        }

        if (bson_iter_init(&arrayIter, &arrayBson))
        {
            while (bson_iter_next(&arrayIter))
            {
                if (BSON_ITER_HOLDS_UTF8(&arrayIter))
                {
                    uint32_t strLength = 0;
                    const char *str = bson_iter_utf8(&arrayIter, &strLength);
                    result.emplace_back(str, strLength);
                }
            }
        }

        return result;
    }

    void MongoDBDocument::setString(const std::string &fieldPath, const std::string &value)
    {
        MONGODB_LOCK_GUARD(m_mutex);
        validateDocument();

        // For simple fields (no dot notation), use direct append
        if (!fieldPath.contains('.'))
        {
            // Create a new document with the field updated
            bson_t *newBson = bson_new();
            if (!newBson)
            {
                throw DBException("E5135FAF11A3", "Failed to create new BSON document", system_utils::captureCallStack());
            }

            // Copy all existing fields except the one we're setting
            bson_iter_t iter;
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

            // Add the new field
            BSON_APPEND_UTF8(newBson, fieldPath.c_str(), value.c_str());

            m_bson.reset(newBson);
            m_idCached = false;
        }
        else
        {
            throw DBException("4WS6KLPHQOE1", "Nested field setting not yet implemented: " + fieldPath, system_utils::captureCallStack());
        }
    }

    void MongoDBDocument::setInt(const std::string &fieldPath, int64_t value)
    {
        MONGODB_LOCK_GUARD(m_mutex);
        validateDocument();

        if (!fieldPath.contains('.'))
        {
            bson_t *newBson = bson_new();
            if (!newBson)
            {
                throw DBException("4OK0OI4LWAP6", "Failed to create new BSON document", system_utils::captureCallStack());
            }

            bson_iter_t iter;
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

            BSON_APPEND_INT64(newBson, fieldPath.c_str(), value);

            m_bson.reset(newBson);
            m_idCached = false;
        }
        else
        {
            throw DBException("FBBC7559CEE6", "Nested field setting not yet implemented: " + fieldPath, system_utils::captureCallStack());
        }
    }

    void MongoDBDocument::setDouble(const std::string &fieldPath, double value)
    {
        MONGODB_LOCK_GUARD(m_mutex);
        validateDocument();

        if (!fieldPath.contains('.'))
        {
            bson_t *newBson = bson_new();
            if (!newBson)
            {
                throw DBException("QP6YJTNDGWER", "Failed to create new BSON document", system_utils::captureCallStack());
            }

            bson_iter_t iter;
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

            BSON_APPEND_DOUBLE(newBson, fieldPath.c_str(), value);

            m_bson.reset(newBson);
            m_idCached = false;
        }
        else
        {
            throw DBException("OY9ZR3WIG8O1", "Nested field setting not yet implemented: " + fieldPath, system_utils::captureCallStack());
        }
    }

    void MongoDBDocument::setBool(const std::string &fieldPath, bool value)
    {
        MONGODB_LOCK_GUARD(m_mutex);
        validateDocument();

        if (!fieldPath.contains('.'))
        {
            bson_t *newBson = bson_new();
            if (!newBson)
            {
                throw DBException("26VOODWJR1ZN", "Failed to create new BSON document", system_utils::captureCallStack());
            }

            bson_iter_t iter;
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

            BSON_APPEND_BOOL(newBson, fieldPath.c_str(), value);

            m_bson.reset(newBson);
            m_idCached = false;
        }
        else
        {
            throw DBException("3B85D3BBFD47", "Nested field setting not yet implemented: " + fieldPath, system_utils::captureCallStack());
        }
    }

    void MongoDBDocument::setBinary(const std::string &fieldPath, const std::vector<uint8_t> &value)
    {
        MONGODB_LOCK_GUARD(m_mutex);
        validateDocument();

        if (!fieldPath.contains('.'))
        {
            bson_t *newBson = bson_new();
            if (!newBson)
            {
                throw DBException("IK72F5Y7D9S4", "Failed to create new BSON document", system_utils::captureCallStack());
            }

            bson_iter_t iter;
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

            BSON_APPEND_BINARY(newBson, fieldPath.c_str(), BSON_SUBTYPE_BINARY,
                               value.data(), static_cast<uint32_t>(value.size()));

            m_bson.reset(newBson);
            m_idCached = false;
        }
        else
        {
            throw DBException("FGZT0IR4HZOC", "Nested field setting not yet implemented: " + fieldPath, system_utils::captureCallStack());
        }
    }

    void MongoDBDocument::setDocument(const std::string &fieldPath, std::shared_ptr<DocumentDBData> doc)
    {
        MONGODB_LOCK_GUARD(m_mutex);
        validateDocument();

        auto mongoDoc = std::dynamic_pointer_cast<MongoDBDocument>(doc);
        if (!mongoDoc)
        {
            throw DBException("0DEA5F5E4607", "Document must be a MongoDBDocument", system_utils::captureCallStack());
        }

        if (!fieldPath.contains('.'))
        {
            bson_t *newBson = bson_new();
            if (!newBson)
            {
                throw DBException("36AFC710EAEB", "Failed to create new BSON document", system_utils::captureCallStack());
            }

            bson_iter_t iter;
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

            BSON_APPEND_DOCUMENT(newBson, fieldPath.c_str(), mongoDoc->getBson());

            m_bson.reset(newBson);
            m_idCached = false;
        }
        else
        {
            throw DBException("8EE4F606BFFE", "Nested field setting not yet implemented: " + fieldPath, system_utils::captureCallStack());
        }
    }

    void MongoDBDocument::setNull(const std::string &fieldPath)
    {
        MONGODB_LOCK_GUARD(m_mutex);
        validateDocument();

        if (!fieldPath.contains('.'))
        {
            bson_t *newBson = bson_new();
            if (!newBson)
            {
                throw DBException("XUEO21OZM8A7", "Failed to create new BSON document", system_utils::captureCallStack());
            }

            bson_iter_t iter;
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

            BSON_APPEND_NULL(newBson, fieldPath.c_str());

            m_bson.reset(newBson);
            m_idCached = false;
        }
        else
        {
            throw DBException("XV7HB9GJYIUE", "Nested field setting not yet implemented: " + fieldPath, system_utils::captureCallStack());
        }
    }

} // namespace cpp_dbc::MongoDB

#endif // USE_MONGODB
