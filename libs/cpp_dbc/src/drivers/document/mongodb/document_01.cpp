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
 * @file document_01.cpp
 * @brief MongoDB MongoDBDocument - Part 1 (private helpers, constructors, operators)
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

    // ============================================================================
    // MongoDBDocument Implementation - Private Helpers
    // ============================================================================

    bool MongoDBDocument::navigateToField(const std::string &fieldPath, bson_iter_t &outIter) const
    {
        MONGODB_LOCK_GUARD(m_mutex);
        validateDocument();

        bson_iter_t iter;
        if (!bson_iter_init(&iter, m_bson.get()))
        {
            return false;
        }

        // Handle dot notation for nested fields
        if (fieldPath.contains('.'))
        {
            return bson_iter_find_descendant(&iter, fieldPath.c_str(), &outIter);
        }
        else
        {
            if (bson_iter_find(&iter, fieldPath.c_str()))
            {
                outIter = iter;
                return true;
            }
            return false;
        }
    }

    void MongoDBDocument::validateDocument() const
    {
        if (!m_bson)
        {
            throw DBException("DBA6A185E250", "Document is not initialized", system_utils::captureCallStack());
        }
    }

    // ============================================================================
    // MongoDBDocument Implementation - Constructors and Operators
    // ============================================================================

    MongoDBDocument::MongoDBDocument()
        : m_bson(bson_new())
    {
        MONGODB_DEBUG("MongoDBDocument::constructor - Creating empty document");
        if (!m_bson)
        {
            throw DBException("17026ED8C0C9", "Failed to create empty BSON document", system_utils::captureCallStack());
        }
        MONGODB_DEBUG("MongoDBDocument::constructor - Done");
    }

    MongoDBDocument::MongoDBDocument(bson_t *bson)
        : m_bson(bson)
    {
        if (!m_bson)
        {
            throw DBException("FA158BABA852", "Cannot create document from null BSON pointer", system_utils::captureCallStack());
        }
    }

    MongoDBDocument::MongoDBDocument(const std::string &json)
    {
        MONGODB_LOCK_GUARD(m_mutex);

        bson_error_t error;
        bson_t *bson = bson_new_from_json(
            reinterpret_cast<const uint8_t *>(json.c_str()),
            static_cast<ssize_t>(json.length()),
            &error);

        if (!bson)
        {
            throw DBException("BA3DA9E3544A", std::string("Failed to parse JSON: ") + error.message, system_utils::captureCallStack());
        }

        m_bson.reset(bson);
    }

    MongoDBDocument::MongoDBDocument(const MongoDBDocument &other)
    {
        MONGODB_LOCK_GUARD(m_mutex);

        if (other.m_bson)
        {
            m_bson.reset(bson_copy(other.m_bson.get()));
            if (!m_bson)
            {
                throw DBException("EE84E381BAF4", "Failed to copy BSON document", system_utils::captureCallStack());
            }
        }
    }

    MongoDBDocument::MongoDBDocument(MongoDBDocument &&other) noexcept
        : m_bson(std::move(other.m_bson)),
          m_cachedId(std::move(other.m_cachedId)),
          m_idCached(other.m_idCached)
    {
        other.m_idCached = false;
    }

    MongoDBDocument &MongoDBDocument::operator=(const MongoDBDocument &other)
    {
        if (this != &other)
        {
            MONGODB_LOCK_GUARD(m_mutex);

            if (other.m_bson)
            {
                m_bson.reset(bson_copy(other.m_bson.get()));
                if (!m_bson)
                {
                    throw DBException("EA9E28036A09", "Failed to copy BSON document", system_utils::captureCallStack());
                }
            }
            else
            {
                m_bson.reset();
            }
            m_idCached = false;
            m_cachedId.clear();
        }
        return *this;
    }

    MongoDBDocument &MongoDBDocument::operator=(MongoDBDocument &&other) noexcept
    {
        if (this != &other)
        {
            m_bson = std::move(other.m_bson);
            m_cachedId = std::move(other.m_cachedId);
            m_idCached = other.m_idCached;
            other.m_idCached = false;
        }
        return *this;
    }

    // ============================================================================
    // MongoDBDocument Implementation - getId, setId
    // ============================================================================

    std::string MongoDBDocument::getId() const
    {
        MONGODB_LOCK_GUARD(m_mutex);
        validateDocument();

        if (m_idCached)
        {
            return m_cachedId;
        }

        bson_iter_t iter;
        if (bson_iter_init_find(&iter, m_bson.get(), "_id"))
        {
            if (BSON_ITER_HOLDS_OID(&iter))
            {
                const bson_oid_t *oid = bson_iter_oid(&iter);
                std::array<char, 25> oidStr{};
                bson_oid_to_string(oid, oidStr.data());
                m_cachedId = oidStr.data();
                m_idCached = true;
                return m_cachedId;
            }
            else if (BSON_ITER_HOLDS_UTF8(&iter))
            {
                uint32_t length = 0;
                const char *str = bson_iter_utf8(&iter, &length);
                m_cachedId = std::string(str, length);
                m_idCached = true;
                return m_cachedId;
            }
        }

        return "";
    }

    void MongoDBDocument::setId(const std::string &id)
    {
        MONGODB_LOCK_GUARD(m_mutex);
        validateDocument();

        // Remove existing _id if present
        bson_t *newBson = bson_new();
        if (!newBson)
        {
            throw DBException("F842E89C6432", "Failed to create new BSON document", system_utils::captureCallStack());
        }

        // Add the new _id first
        bson_oid_t oid;
        if (bson_oid_is_valid(id.c_str(), id.length()))
        {
            bson_oid_init_from_string(&oid, id.c_str());
            BSON_APPEND_OID(newBson, "_id", &oid);
        }
        else
        {
            BSON_APPEND_UTF8(newBson, "_id", id.c_str());
        }

        // Copy all other fields
        bson_iter_t iter;
        if (bson_iter_init(&iter, m_bson.get()))
        {
            while (bson_iter_next(&iter))
            {
                const char *key = bson_iter_key(&iter);
                if (std::string_view(key) != "_id")
                {
                    bson_append_iter(newBson, key, -1, &iter);
                }
            }
        }

        m_bson.reset(newBson);
        m_cachedId = id;
        m_idCached = true;
    }

    // ============================================================================
    // MongoDBDocument Implementation - toJson, toJsonPretty, fromJson
    // ============================================================================

    std::string MongoDBDocument::toJson() const
    {
        MONGODB_LOCK_GUARD(m_mutex);
        validateDocument();

        size_t length = 0;
        char *json = bson_as_relaxed_extended_json(m_bson.get(), &length);
        if (!json)
        {
            throw DBException("B41282C21719", "Failed to convert document to JSON", system_utils::captureCallStack());
        }

        std::string result(json, length);
        bson_free(json);
        return result;
    }

    std::string MongoDBDocument::toJsonPretty() const
    {
        MONGODB_LOCK_GUARD(m_mutex);
        validateDocument();

        size_t length = 0;
        char *json = bson_as_canonical_extended_json(m_bson.get(), &length);
        if (!json)
        {
            throw DBException("9D9EA6A742A4", "Failed to convert document to JSON", system_utils::captureCallStack());
        }

        std::string result(json, length);
        bson_free(json);
        return result;
    }

    void MongoDBDocument::fromJson(const std::string &json)
    {
        MONGODB_LOCK_GUARD(m_mutex);

        bson_error_t error;
        bson_t *bson = bson_new_from_json(
            reinterpret_cast<const uint8_t *>(json.c_str()),
            static_cast<ssize_t>(json.length()),
            &error);

        if (!bson)
        {
            throw DBException("671F94F63F3D", std::string("Failed to parse JSON: ") + error.message, system_utils::captureCallStack());
        }

        m_bson.reset(bson);
        m_idCached = false;
        m_cachedId.clear();
    }

} // namespace cpp_dbc::MongoDB

#endif // USE_MONGODB
