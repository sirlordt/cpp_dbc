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
 * @brief MongoDB MongoDBDocument - Part 1 (private nothrow constructors, private helpers, throwing wrappers: getId, setId, toJson, toJsonPretty, fromJson)
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
    // MongoDBDocument Implementation - Nothrow constructors (always available)
    // Public for std::make_shared access, but effectively private via PrivateCtorTag.
    // ============================================================================

    // Default constructor — creates an empty document (m_bson == nullptr until factory assigns a handle).
    // Empty body is intentional and cannot use = default: the C++ standard only allows defaulting
    // the default constructor (no parameters), copy/move constructors, and copy/move assignment
    // operators. This constructor takes PrivateCtorTag + std::nothrow_t, so it is not eligible.
    MongoDBDocument::MongoDBDocument(MongoDBDocument::PrivateCtorTag, std::nothrow_t) noexcept
    {
        // Intentionally empty — all members use in-class initializers
    }

    MongoDBDocument::MongoDBDocument(MongoDBDocument::PrivateCtorTag, std::nothrow_t, bson_t *bson) noexcept
        : m_bson(bson)
    {
        if (!m_bson)
        {
            m_initFailed = true;
            m_initError = DBException("FD8E3A3DQYXQ", "Cannot create document from null BSON pointer", system_utils::captureCallStack());
        }
    }

    MongoDBDocument::MongoDBDocument(MongoDBDocument::PrivateCtorTag, std::nothrow_t, const std::string &json) noexcept
    {
        DB_DRIVER_LOCK_GUARD(m_mutex);

        bson_error_t error;
        bson_t *bson = bson_new_from_json(
            reinterpret_cast<const uint8_t *>(json.c_str()),
            static_cast<ssize_t>(json.length()),
            &error);

        if (!bson)
        {
            m_initFailed = true;
            m_initError = DBException("QSA3OO6XGI66", std::string("Failed to parse JSON: ") + error.message, system_utils::captureCallStack());
            return;
        }

        m_bson.reset(bson);
    }

    // ============================================================================
    // MongoDBDocument Implementation - Private Helpers (nothrow)
    // ============================================================================

    expected<bool, DBException> MongoDBDocument::navigateToField(std::nothrow_t, const std::string &fieldPath, bson_iter_t &outIter) const noexcept
    {
        DB_DRIVER_LOCK_GUARD(m_mutex);

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

    expected<void, DBException> MongoDBDocument::validateDocument(std::nothrow_t) const noexcept
    {
        if (!m_bson)
        {
            return unexpected(DBException("DBA6A185E250", "Document is not initialized", system_utils::captureCallStack()));
        }
        return {};
    }

    // ============================================================================
    // MongoDBDocument Implementation - Throwing API wrappers
    //                                  (only when exceptions are enabled)
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

    // ============================================================================
    // MongoDBDocument Implementation - Nothrow factories (public, always available)
    // ============================================================================

    expected<std::shared_ptr<MongoDBDocument>, DBException>
    MongoDBDocument::create(std::nothrow_t) noexcept
    {
        // The nothrow constructor stores init errors in m_initFailed/m_initError
        // rather than throwing, so no try/catch is needed here.
        auto obj = std::make_shared<MongoDBDocument>(PrivateCtorTag{}, std::nothrow);
        obj->m_bson.reset(bson_new());
        if (!obj->m_bson)
        {
            return unexpected<DBException>(DBException(
                "LPLGD9BE9NM0",
                "Failed to create empty BSON document",
                system_utils::captureCallStack()));
        }
        return obj;
    }

    expected<std::shared_ptr<MongoDBDocument>, DBException>
    MongoDBDocument::create(std::nothrow_t, bson_t *bson) noexcept
    {
        // The nothrow constructor stores init errors in m_initFailed/m_initError
        // rather than throwing, so no try/catch is needed here.
        auto obj = std::make_shared<MongoDBDocument>(PrivateCtorTag{}, std::nothrow, bson);
        if (obj->m_initFailed)
        {
            return unexpected<DBException>(obj->m_initError);
        }
        return obj;
    }

    expected<std::shared_ptr<MongoDBDocument>, DBException>
    MongoDBDocument::create(std::nothrow_t, const std::string &json) noexcept
    {
        // The nothrow constructor stores init errors in m_initFailed/m_initError
        // rather than throwing, so no try/catch is needed here.
        auto obj = std::make_shared<MongoDBDocument>(PrivateCtorTag{}, std::nothrow, json);
        if (obj->m_initFailed)
        {
            return unexpected<DBException>(obj->m_initError);
        }
        return obj;
    }

    expected<std::shared_ptr<MongoDBDocument>, DBException>
    MongoDBDocument::copyFrom(std::nothrow_t, const bson_t *bson) noexcept
    {
        if (!bson)
        {
            return unexpected<DBException>(DBException(
                "Z99M25OOHIBD",
                "Cannot copy from null BSON pointer",
                system_utils::captureCallStack()));
        }

        bson_t *copy = bson_copy(bson);
        if (!copy)
        {
            return unexpected<DBException>(DBException(
                "UYBXP3TMVRMC",
                "Failed to copy BSON document",
                system_utils::captureCallStack()));
        }

        return create(std::nothrow, copy);
    }

} // namespace cpp_dbc::MongoDB

#endif // USE_MONGODB
