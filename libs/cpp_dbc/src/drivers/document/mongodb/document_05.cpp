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
 * @file document_05.cpp
 * @brief MongoDB MongoDBDocument - Part 5 (nothrow API: getId, setId, toJson, toJsonPretty, fromJson)
 */

#include "cpp_dbc/drivers/document/driver_mongodb.hpp"

#if USE_MONGODB

#include <array>
#include "cpp_dbc/common/system_utils.hpp"
#include "mongodb_internal.hpp"

namespace cpp_dbc::MongoDB
{

    // ====================================================================
    // NOTHROW API - getId, setId, toJson, toJsonPretty, fromJson (real implementations)
    // ====================================================================

    expected<std::string, DBException> MongoDBDocument::getId(std::nothrow_t) const noexcept
    {
        DB_DRIVER_LOCK_GUARD(m_mutex);

        auto v = validateDocument(std::nothrow);
        if (!v.has_value())
        {
            return unexpected<DBException>(v.error());
        }

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

        return std::string{};
    }

    expected<void, DBException> MongoDBDocument::setId(std::nothrow_t, const std::string &id) noexcept
    {
        DB_DRIVER_LOCK_GUARD(m_mutex);

        auto v = validateDocument(std::nothrow);
        if (!v.has_value())
        {
            return unexpected<DBException>(v.error());
        }

        bson_t *newBson = bson_new();
        if (!newBson)
        {
            return unexpected<DBException>(DBException(
                "AKMSOBSD178M",
                "Failed to create new BSON document",
                system_utils::captureCallStack()));
        }

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
        return {};
    }

    expected<std::string, DBException> MongoDBDocument::toJson(std::nothrow_t) const noexcept
    {
        DB_DRIVER_LOCK_GUARD(m_mutex);

        auto v = validateDocument(std::nothrow);
        if (!v.has_value())
        {
            return unexpected<DBException>(v.error());
        }

        size_t length = 0;
        char *json = bson_as_relaxed_extended_json(m_bson.get(), &length);
        if (!json)
        {
            return unexpected<DBException>(DBException(
                "8X5RXA1WE1JA",
                "Failed to convert document to JSON",
                system_utils::captureCallStack()));
        }

        std::string result(json, length);
        bson_free(json);
        return result;
    }

    expected<std::string, DBException> MongoDBDocument::toJsonPretty(std::nothrow_t) const noexcept
    {
        DB_DRIVER_LOCK_GUARD(m_mutex);

        auto v = validateDocument(std::nothrow);
        if (!v.has_value())
        {
            return unexpected<DBException>(v.error());
        }

        size_t length = 0;
        char *json = bson_as_canonical_extended_json(m_bson.get(), &length);
        if (!json)
        {
            return unexpected<DBException>(DBException(
                "QPNHC42C5L6Y",
                "Failed to convert document to JSON",
                system_utils::captureCallStack()));
        }

        std::string result(json, length);
        bson_free(json);
        return result;
    }

    expected<void, DBException> MongoDBDocument::fromJson(std::nothrow_t, const std::string &json) noexcept
    {
        DB_DRIVER_LOCK_GUARD(m_mutex);

        bson_error_t error;
        bson_t *bson = bson_new_from_json(
            reinterpret_cast<const uint8_t *>(json.c_str()),
            static_cast<ssize_t>(json.length()),
            &error);

        if (!bson)
        {
            return unexpected<DBException>(DBException(
                "3CKKBLN3Q4OW",
                std::string("Failed to parse JSON: ") + error.message,
                system_utils::captureCallStack()));
        }

        m_bson.reset(bson);
        m_idCached = false;
        m_cachedId.clear();
        return {};
    }

} // namespace cpp_dbc::MongoDB

#endif // USE_MONGODB
