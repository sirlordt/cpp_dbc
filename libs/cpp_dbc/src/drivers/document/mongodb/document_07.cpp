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
 * @file document_07.cpp
 * @brief MongoDB MongoDBDocument - Part 7 (nothrow API: getDocumentArray, getStringArray,
 *        setters, hasField, isNull, removeField, getFieldNames, clone, clear, isEmpty,
 *        getBson, getBsonMutable)
 */

#include "cpp_dbc/drivers/document/driver_mongodb.hpp"

#if USE_MONGODB

#include "cpp_dbc/common/system_utils.hpp"
#include "mongodb_internal.hpp"

namespace cpp_dbc::MongoDB
{

    // ====================================================================
    // NOTHROW API - getDocumentArray, getStringArray (real implementations)
    // ====================================================================

    expected<std::vector<std::shared_ptr<DocumentDBData>>, DBException> MongoDBDocument::getDocumentArray(
        std::nothrow_t, const std::string &fieldPath) const noexcept
    {
        // Default: tolerant mode (skip non-document elements)
        return getDocumentArray(std::nothrow, fieldPath, false);
    }

    expected<std::vector<std::string>, DBException> MongoDBDocument::getStringArray(
        std::nothrow_t, const std::string &fieldPath) const noexcept
    {
        // Default: tolerant mode (skip non-string elements)
        return getStringArray(std::nothrow, fieldPath, false);
    }

    expected<std::vector<std::shared_ptr<DocumentDBData>>, DBException> MongoDBDocument::getDocumentArray(
        std::nothrow_t, const std::string &fieldPath, bool strict) const noexcept
    {
        MONGODB_LOCK_GUARD(m_mutex);

        bson_iter_t iter;
        auto nav = navigateToField(std::nothrow, fieldPath, iter);
        if (!nav.has_value())
        {
            return unexpected<DBException>(nav.error());
        }
        if (!*nav)
        {
            return unexpected<DBException>(DBException(
                "ETFVXIKSNL8N",
                "Field not found: " + fieldPath,
                system_utils::captureCallStack()));
        }

        if (!BSON_ITER_HOLDS_ARRAY(&iter))
        {
            return unexpected<DBException>(DBException(
                "GTXYN62PIXJE",
                "Field is not an array: " + fieldPath,
                system_utils::captureCallStack()));
        }

        std::vector<std::shared_ptr<DocumentDBData>> result;

        bson_iter_t arrayIter;
        const uint8_t *data = nullptr;
        uint32_t length = 0;
        bson_iter_array(&iter, &length, &data);

        bson_t arrayBson;
        if (!bson_init_static(&arrayBson, data, length))
        {
            return unexpected<DBException>(DBException(
                "NF2R6MV8KK5L",
                "Failed to initialize array BSON",
                system_utils::captureCallStack()));
        }

        if (bson_iter_init(&arrayIter, &arrayBson))
        {
            size_t elementIndex = 0;
            while (bson_iter_next(&arrayIter))
            {
                if (BSON_ITER_HOLDS_DOCUMENT(&arrayIter))
                {
                    const uint8_t *docData = nullptr;
                    uint32_t docLength = 0;
                    bson_iter_document(&arrayIter, &docLength, &docData);

                    bson_t *subdoc = bson_new_from_data(docData, docLength);
                    if (!subdoc)
                    {
                        return unexpected<DBException>(DBException(
                            "6F7A8B9C0D1F",
                            "Failed to construct subdocument at index " + std::to_string(elementIndex) + " in array field: " + fieldPath,
                            system_utils::captureCallStack()));
                    }
                    // Use the private nothrow factory to construct the document without invoking
                    // the throwing constructor — required to stay within this method's noexcept contract.
                    // subdoc is guaranteed non-null here (bson_new_from_data returned it above).
                    auto subdocResult = MongoDBDocument::create(std::nothrow, subdoc);
                    if (!subdocResult.has_value())
                    {
                        return unexpected<DBException>(subdocResult.error());
                    }
                    result.push_back(subdocResult.value());
                }
                else if (strict)
                {
                    return unexpected<DBException>(DBException(
                        "OBCZEIAED7KA",
                        "Unexpected element type at index " + std::to_string(elementIndex) + " in array field: " + fieldPath + " (expected document)",
                        system_utils::captureCallStack()));
                }
                // If not strict, skip non-document elements
                elementIndex++;
            }
        }

        return result;
    }

    expected<std::vector<std::string>, DBException> MongoDBDocument::getStringArray(
        std::nothrow_t, const std::string &fieldPath, bool strict) const noexcept
    {
        MONGODB_LOCK_GUARD(m_mutex);

        bson_iter_t iter;
        auto nav = navigateToField(std::nothrow, fieldPath, iter);
        if (!nav.has_value())
        {
            return unexpected<DBException>(nav.error());
        }
        if (!*nav)
        {
            return unexpected<DBException>(DBException(
                "OZAG3C66FY2I",
                "Field not found: " + fieldPath,
                system_utils::captureCallStack()));
        }

        if (!BSON_ITER_HOLDS_ARRAY(&iter))
        {
            return unexpected<DBException>(DBException(
                "R8T5JVV0E4D0",
                "Field is not an array: " + fieldPath,
                system_utils::captureCallStack()));
        }

        std::vector<std::string> result;

        bson_iter_t arrayIter;
        const uint8_t *data = nullptr;
        uint32_t length = 0;
        bson_iter_array(&iter, &length, &data);

        bson_t arrayBson;
        if (!bson_init_static(&arrayBson, data, length))
        {
            return unexpected<DBException>(DBException(
                "I93Q6XI9RNS5",
                "Failed to initialize array BSON",
                system_utils::captureCallStack()));
        }

        if (bson_iter_init(&arrayIter, &arrayBson))
        {
            size_t elementIndex = 0;
            while (bson_iter_next(&arrayIter))
            {
                if (BSON_ITER_HOLDS_UTF8(&arrayIter))
                {
                    uint32_t strLength = 0;
                    const char *str = bson_iter_utf8(&arrayIter, &strLength);
                    result.emplace_back(str, strLength);
                }
                else if (strict)
                {
                    return unexpected<DBException>(DBException(
                        "8B9C0D1E2F3G",
                        "Unexpected element type at index " + std::to_string(elementIndex) + " in array field: " + fieldPath + " (expected string)",
                        system_utils::captureCallStack()));
                }
                // If not strict, skip non-string elements
                elementIndex++;
            }
        }

        return result;
    }

    // ====================================================================
    // NOTHROW API - setString, setInt, setDouble, setBool, setBinary, setDocument, setNull (real implementations)
    // ====================================================================

    expected<void, DBException> MongoDBDocument::setString(
        std::nothrow_t, const std::string &fieldPath, const std::string &value) noexcept
    {
        MONGODB_LOCK_GUARD(m_mutex);

        auto v = validateDocument(std::nothrow);
        if (!v.has_value())
        {
            return unexpected<DBException>(v.error());
        }

        if (!fieldPath.contains('.'))
        {
            bson_t *newBson = bson_new();
            if (!newBson)
            {
                return unexpected<DBException>(DBException(
                    "U65F7ZDFKY7H",
                    "Failed to create new BSON document",
                    system_utils::captureCallStack()));
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

            BSON_APPEND_UTF8(newBson, fieldPath.c_str(), value.c_str());

            m_bson.reset(newBson);
            m_idCached = false;
            return {};
        }
        else
        {
            return unexpected<DBException>(DBException(
                "65VEBP1IS1Y0",
                "Nested field setting not yet implemented: " + fieldPath,
                system_utils::captureCallStack()));
        }
    }

    expected<void, DBException> MongoDBDocument::setInt(
        std::nothrow_t, const std::string &fieldPath, int64_t value) noexcept
    {
        MONGODB_LOCK_GUARD(m_mutex);

        auto v = validateDocument(std::nothrow);
        if (!v.has_value())
        {
            return unexpected<DBException>(v.error());
        }

        if (!fieldPath.contains('.'))
        {
            bson_t *newBson = bson_new();
            if (!newBson)
            {
                return unexpected<DBException>(DBException(
                    "MAV15XIGS4JI",
                    "Failed to create new BSON document",
                    system_utils::captureCallStack()));
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
            return {};
        }
        else
        {
            return unexpected<DBException>(DBException(
                "FN06K2FOPAZH",
                "Nested field setting not yet implemented: " + fieldPath,
                system_utils::captureCallStack()));
        }
    }

    expected<void, DBException> MongoDBDocument::setDouble(
        std::nothrow_t, const std::string &fieldPath, double value) noexcept
    {
        MONGODB_LOCK_GUARD(m_mutex);

        auto v = validateDocument(std::nothrow);
        if (!v.has_value())
        {
            return unexpected<DBException>(v.error());
        }

        if (!fieldPath.contains('.'))
        {
            bson_t *newBson = bson_new();
            if (!newBson)
            {
                return unexpected<DBException>(DBException(
                    "JN32NTXLYJYY",
                    "Failed to create new BSON document",
                    system_utils::captureCallStack()));
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
            return {};
        }
        else
        {
            return unexpected<DBException>(DBException(
                "YI8YB44K1EDP",
                "Nested field setting not yet implemented: " + fieldPath,
                system_utils::captureCallStack()));
        }
    }

    expected<void, DBException> MongoDBDocument::setBool(
        std::nothrow_t, const std::string &fieldPath, bool value) noexcept
    {
        MONGODB_LOCK_GUARD(m_mutex);

        auto v = validateDocument(std::nothrow);
        if (!v.has_value())
        {
            return unexpected<DBException>(v.error());
        }

        if (!fieldPath.contains('.'))
        {
            bson_t *newBson = bson_new();
            if (!newBson)
            {
                return unexpected<DBException>(DBException(
                    "SU6XDSSN0Z3Q",
                    "Failed to create new BSON document",
                    system_utils::captureCallStack()));
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
            return {};
        }
        else
        {
            return unexpected<DBException>(DBException(
                "MOE65CWVW1AE",
                "Nested field setting not yet implemented: " + fieldPath,
                system_utils::captureCallStack()));
        }
    }

    expected<void, DBException> MongoDBDocument::setBinary(
        std::nothrow_t, const std::string &fieldPath, const std::vector<uint8_t> &value) noexcept
    {
        MONGODB_LOCK_GUARD(m_mutex);

        auto v = validateDocument(std::nothrow);
        if (!v.has_value())
        {
            return unexpected<DBException>(v.error());
        }

        if (!fieldPath.contains('.'))
        {
            bson_t *newBson = bson_new();
            if (!newBson)
            {
                return unexpected<DBException>(DBException(
                    "E6GJ9XHDC3MQ",
                    "Failed to create new BSON document",
                    system_utils::captureCallStack()));
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
            return {};
        }
        else
        {
            return unexpected<DBException>(DBException(
                "N5KZ8BVYE4OU",
                "Nested field setting not yet implemented: " + fieldPath,
                system_utils::captureCallStack()));
        }
    }

    expected<void, DBException> MongoDBDocument::setDocument(
        std::nothrow_t, const std::string &fieldPath, std::shared_ptr<DocumentDBData> doc) noexcept
    {
        MONGODB_LOCK_GUARD(m_mutex);

        auto v = validateDocument(std::nothrow);
        if (!v.has_value())
        {
            return unexpected<DBException>(v.error());
        }

        auto mongoDoc = std::dynamic_pointer_cast<MongoDBDocument>(doc);
        if (!mongoDoc)
        {
            return unexpected<DBException>(DBException(
                "S0ON3YSPT9JH",
                "Document must be a MongoDBDocument",
                system_utils::captureCallStack()));
        }

        if (!fieldPath.contains('.'))
        {
            bson_t *newBson = bson_new();
            if (!newBson)
            {
                return unexpected<DBException>(DBException(
                    "T9NM2XROS8IG",
                    "Failed to create new BSON document",
                    system_utils::captureCallStack()));
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
            return {};
        }
        else
        {
            return unexpected<DBException>(DBException(
                "U8ML1WQNR7HF",
                "Nested field setting not yet implemented: " + fieldPath,
                system_utils::captureCallStack()));
        }
    }

    expected<void, DBException> MongoDBDocument::setNull(
        std::nothrow_t, const std::string &fieldPath) noexcept
    {
        MONGODB_LOCK_GUARD(m_mutex);

        auto v = validateDocument(std::nothrow);
        if (!v.has_value())
        {
            return unexpected<DBException>(v.error());
        }

        if (!fieldPath.contains('.'))
        {
            bson_t *newBson = bson_new();
            if (!newBson)
            {
                return unexpected<DBException>(DBException(
                    "Y4IH7SMJL3DB",
                    "Failed to create new BSON document",
                    system_utils::captureCallStack()));
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
            return {};
        }
        else
        {
            return unexpected<DBException>(DBException(
                "Z3HG6RLIK2CA",
                "Nested field setting not yet implemented: " + fieldPath,
                system_utils::captureCallStack()));
        }
    }

    // ====================================================================
    // NOTHROW API - hasField, isNull, removeField, getFieldNames (real implementations)
    // ====================================================================

    expected<bool, DBException> MongoDBDocument::hasField(
        std::nothrow_t, const std::string &fieldPath) const noexcept
    {
        MONGODB_LOCK_GUARD(m_mutex);

        if (!m_bson)
        {
            return false;
        }

        bson_iter_t iter;
        auto nav = navigateToField(std::nothrow, fieldPath, iter);
        if (!nav.has_value())
        {
            return unexpected<DBException>(nav.error());
        }
        return *nav;
    }

    expected<bool, DBException> MongoDBDocument::isNull(
        std::nothrow_t, const std::string &fieldPath) const noexcept
    {
        MONGODB_LOCK_GUARD(m_mutex);

        bson_iter_t iter;
        auto nav = navigateToField(std::nothrow, fieldPath, iter);
        if (!nav.has_value())
        {
            return unexpected<DBException>(nav.error());
        }
        if (!*nav)
        {
            return true; // Field doesn't exist, treat as null
        }

        return static_cast<bool>(BSON_ITER_HOLDS_NULL(&iter));
    }

    expected<bool, DBException> MongoDBDocument::removeField(
        std::nothrow_t, const std::string &fieldPath) noexcept
    {
        MONGODB_LOCK_GUARD(m_mutex);

        auto v = validateDocument(std::nothrow);
        if (!v.has_value())
        {
            return unexpected<DBException>(v.error());
        }

        if (fieldPath.contains('.'))
        {
            return unexpected<DBException>(DBException(
                "H5ZY8JDCC4US",
                "Nested field removal not yet implemented: " + fieldPath,
                system_utils::captureCallStack()));
        }

        bson_iter_t iter;
        auto nav = navigateToField(std::nothrow, fieldPath, iter);
        if (!nav.has_value())
        {
            return unexpected<DBException>(nav.error());
        }
        if (!*nav)
        {
            return false;
        }

        bson_t *newBson = bson_new();
        if (!newBson)
        {
            return unexpected<DBException>(DBException(
                "I4YX7ICBB3TR",
                "Failed to create new BSON document",
                system_utils::captureCallStack()));
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

    expected<std::vector<std::string>, DBException> MongoDBDocument::getFieldNames(
        std::nothrow_t) const noexcept
    {
        MONGODB_LOCK_GUARD(m_mutex);

        auto v = validateDocument(std::nothrow);
        if (!v.has_value())
        {
            return unexpected<DBException>(v.error());
        }

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

    // ====================================================================
    // NOTHROW API - clone, clear, isEmpty (real implementations)
    // ====================================================================

    expected<std::shared_ptr<DocumentDBData>, DBException> MongoDBDocument::clone(std::nothrow_t) const noexcept
    {
        MONGODB_LOCK_GUARD(m_mutex);

        auto v = validateDocument(std::nothrow);
        if (!v.has_value())
        {
            return unexpected<DBException>(v.error());
        }

        bson_t *copy = bson_copy(m_bson.get());
        if (!copy)
        {
            return unexpected<DBException>(DBException(
                "ILK5IWU0MFPB",
                "Failed to clone document",
                system_utils::captureCallStack()));
        }

        // Use the private nothrow factory to construct the document without invoking
        // the throwing constructor — required to stay within this method's noexcept contract.
        // copy is guaranteed non-null here (bson_copy returned it above).
        auto docResult = MongoDBDocument::create(std::nothrow, copy);
        if (!docResult.has_value())
        {
            return unexpected<DBException>(docResult.error());
        }
        return std::static_pointer_cast<DocumentDBData>(docResult.value());
    }

    expected<void, DBException> MongoDBDocument::clear(std::nothrow_t) noexcept
    {
        MONGODB_LOCK_GUARD(m_mutex);

        bson_t *empty = bson_new();
        if (!empty)
        {
            return unexpected<DBException>(DBException(
                "P7RQ0BVU46MK",
                "Failed to create empty BSON document",
                system_utils::captureCallStack()));
        }

        m_bson.reset(empty);
        m_idCached = false;
        m_cachedId.clear();
        return {};
    }

    expected<bool, DBException> MongoDBDocument::isEmpty(std::nothrow_t) const noexcept
    {
        MONGODB_LOCK_GUARD(m_mutex);

        if (!m_bson)
        {
            return true;
        }

        return bson_count_keys(m_bson.get()) == 0;
    }

    // ====================================================================
    // NOTHROW API - MongoDB-specific: getBson, getBsonMutable
    // ====================================================================

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

} // namespace cpp_dbc::MongoDB

#endif // USE_MONGODB
