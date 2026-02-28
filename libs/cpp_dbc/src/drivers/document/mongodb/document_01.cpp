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
    // MongoDBDocument Implementation - Private Helpers (nothrow)
    // ============================================================================

    expected<void, DBException> MongoDBDocument::validateDocument(std::nothrow_t) const noexcept
    {
        if (!m_bson)
        {
            return unexpected(DBException("DBA6A185E250", "Document is not initialized", system_utils::captureCallStack()));
        }
        return {};
    }

    expected<bool, DBException> MongoDBDocument::navigateToField(std::nothrow_t, const std::string &fieldPath, bson_iter_t &outIter) const noexcept
    {
        MONGODB_LOCK_GUARD(m_mutex);

        auto v = validateDocument(std::nothrow);
        if (!v.has_value())
        {
            return unexpected(v.error());
        }

        bson_iter_t iter;
        if (!bson_iter_init(&iter, m_bson.get()))
        {
            return false;
        }

        // Handle dot notation for nested fields
        if (fieldPath.contains('.'))
        {
            return static_cast<bool>(bson_iter_find_descendant(&iter, fieldPath.c_str(), &outIter));
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

    // ============================================================================
    // MongoDBDocument Implementation - Constructors and Operators
    // ============================================================================

    MongoDBDocument::MongoDBDocument()
        : m_bson(bson_new())
    {
        MONGODB_DEBUG("MongoDBDocument::constructor - Creating empty document");
        if (!m_bson)
        {
            throw DBException("LPLGD9BE9NM0", "Failed to create empty BSON document", system_utils::captureCallStack());
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
                    throw DBException("OWX8BG6W9Q7I", "Failed to copy BSON document", system_utils::captureCallStack());
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
    // MongoDBDocument Implementation - getId, setId (throwing wrappers)
    // ============================================================================

    #ifdef __cpp_exceptions
    std::string MongoDBDocument::getId() const
    {
        auto r = getId(std::nothrow);
        if (!r.has_value())
        {
            throw r.error();
        }
        return *r;
    }

    void MongoDBDocument::setId(const std::string &id)
    {
        auto r = setId(std::nothrow, id);
        if (!r.has_value())
        {
            throw r.error();
        }
    }

    // ============================================================================
    // MongoDBDocument Implementation - toJson, toJsonPretty, fromJson (throwing wrappers)
    // ============================================================================

    std::string MongoDBDocument::toJson() const
    {
        auto r = toJson(std::nothrow);
        if (!r.has_value())
        {
            throw r.error();
        }
        return *r;
    }

    std::string MongoDBDocument::toJsonPretty() const
    {
        auto r = toJsonPretty(std::nothrow);
        if (!r.has_value())
        {
            throw r.error();
        }
        return *r;
    }

    void MongoDBDocument::fromJson(const std::string &json)
    {
        auto r = fromJson(std::nothrow, json);
        if (!r.has_value())
        {
            throw r.error();
        }
    }
    #endif // __cpp_exceptions

} // namespace cpp_dbc::MongoDB

#endif // USE_MONGODB
