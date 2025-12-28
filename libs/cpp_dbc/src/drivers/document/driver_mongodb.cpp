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
 * @file driver_mongodb.cpp
 * @brief MongoDB database driver implementation
 */

#include "cpp_dbc/drivers/document/driver_mongodb.hpp"

#if USE_MONGODB

#include <algorithm>
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <cpp_dbc/common/system_utils.hpp>

// Debug output is controlled by -DDEBUG_MONGODB=1 or -DDEBUG_ALL=1 CMake option
#if (defined(DEBUG_MONGODB) && DEBUG_MONGODB) || (defined(DEBUG_ALL) && DEBUG_ALL)
#define MONGODB_DEBUG(x) std::cout << "[MongoDB] " << x << std::endl
#else
#define MONGODB_DEBUG(x)
#endif

namespace cpp_dbc
{
    namespace MongoDB
    {
        // ============================================================================
        // Static member initialization
        // ============================================================================

        std::once_flag MongoDBDriver::s_initFlag;
        std::atomic<bool> MongoDBDriver::s_initialized{false};

        // ============================================================================
        // MongoDBDocument Implementation
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

        void MongoDBDocument::validateDocument() const
        {
            if (!m_bson)
            {
                throw DBException("DBA6A185E250", "Document is not initialized", system_utils::captureCallStack());
            }
        }

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
            if (fieldPath.find('.') != std::string::npos)
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
                    char oidStr[25];
                    bson_oid_to_string(oid, oidStr);
                    m_cachedId = oidStr;
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
                    if (std::strcmp(key, "_id") != 0)
                    {
                        bson_append_iter(newBson, key, -1, &iter);
                    }
                }
            }

            m_bson.reset(newBson);
            m_cachedId = id;
            m_idCached = true;
        }

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

        std::string MongoDBDocument::getString(const std::string &fieldPath) const
        {
            MONGODB_LOCK_GUARD(m_mutex);

            bson_iter_t iter;
            if (!navigateToField(fieldPath, iter))
            {
                throw DBException("E0C6A6D72CFB", "Field not found: " + fieldPath, system_utils::captureCallStack());
            }

            if (!BSON_ITER_HOLDS_UTF8(&iter))
            {
                throw DBException("0776DCEC534E", "Field is not a string: " + fieldPath, system_utils::captureCallStack());
            }

            uint32_t length = 0;
            const char *str = bson_iter_utf8(&iter, &length);
            return std::string(str, length);
        }

        int64_t MongoDBDocument::getInt(const std::string &fieldPath) const
        {
            MONGODB_LOCK_GUARD(m_mutex);

            bson_iter_t iter;
            if (!navigateToField(fieldPath, iter))
            {
                throw DBException("AC3BEA0AD9DC", "Field not found: " + fieldPath, system_utils::captureCallStack());
            }

            if (BSON_ITER_HOLDS_INT32(&iter))
            {
                return bson_iter_int32(&iter);
            }
            else if (BSON_ITER_HOLDS_INT64(&iter))
            {
                return bson_iter_int64(&iter);
            }
            else
            {
                throw DBException("106E13A1A80D", "Field is not an integer: " + fieldPath, system_utils::captureCallStack());
            }
        }

        double MongoDBDocument::getDouble(const std::string &fieldPath) const
        {
            MONGODB_LOCK_GUARD(m_mutex);

            bson_iter_t iter;
            if (!navigateToField(fieldPath, iter))
            {
                throw DBException("4F6F3CC06756", "Field not found: " + fieldPath, system_utils::captureCallStack());
            }

            if (BSON_ITER_HOLDS_DOUBLE(&iter))
            {
                return bson_iter_double(&iter);
            }
            else if (BSON_ITER_HOLDS_INT32(&iter))
            {
                return static_cast<double>(bson_iter_int32(&iter));
            }
            else if (BSON_ITER_HOLDS_INT64(&iter))
            {
                return static_cast<double>(bson_iter_int64(&iter));
            }
            else
            {
                throw DBException("D14D13293D6E", "Field is not a number: " + fieldPath, system_utils::captureCallStack());
            }
        }

        bool MongoDBDocument::getBool(const std::string &fieldPath) const
        {
            MONGODB_LOCK_GUARD(m_mutex);

            bson_iter_t iter;
            if (!navigateToField(fieldPath, iter))
            {
                throw DBException("D3DDAB280443", "Field not found: " + fieldPath, system_utils::captureCallStack());
            }

            if (!BSON_ITER_HOLDS_BOOL(&iter))
            {
                throw DBException("89845A16FE9B", "Field is not a boolean: " + fieldPath, system_utils::captureCallStack());
            }

            return bson_iter_bool(&iter);
        }

        std::vector<uint8_t> MongoDBDocument::getBinary(const std::string &fieldPath) const
        {
            MONGODB_LOCK_GUARD(m_mutex);

            bson_iter_t iter;
            if (!navigateToField(fieldPath, iter))
            {
                throw DBException("8C871E66955A", "Field not found: " + fieldPath, system_utils::captureCallStack());
            }

            if (!BSON_ITER_HOLDS_BINARY(&iter))
            {
                throw DBException("25536C66C3CE", "Field is not binary: " + fieldPath, system_utils::captureCallStack());
            }

            bson_subtype_t subtype;
            uint32_t length = 0;
            const uint8_t *data = nullptr;
            bson_iter_binary(&iter, &subtype, &length, &data);

            return std::vector<uint8_t>(data, data + length);
        }

        std::shared_ptr<DocumentDBData> MongoDBDocument::getDocument(const std::string &fieldPath) const
        {
            MONGODB_LOCK_GUARD(m_mutex);

            bson_iter_t iter;
            if (!navigateToField(fieldPath, iter))
            {
                throw DBException("79B0503E9864", "Field not found: " + fieldPath, system_utils::captureCallStack());
            }

            if (!BSON_ITER_HOLDS_DOCUMENT(&iter))
            {
                throw DBException("640CC8227742", "Field is not a document: " + fieldPath, system_utils::captureCallStack());
            }

            const uint8_t *data = nullptr;
            uint32_t length = 0;
            bson_iter_document(&iter, &length, &data);

            bson_t *subdoc = bson_new_from_data(data, length);
            if (!subdoc)
            {
                throw DBException("911070CDD871", "Failed to extract subdocument", system_utils::captureCallStack());
            }

            return std::make_shared<MongoDBDocument>(subdoc);
        }

        std::vector<std::shared_ptr<DocumentDBData>> MongoDBDocument::getDocumentArray(const std::string &fieldPath) const
        {
            MONGODB_LOCK_GUARD(m_mutex);

            bson_iter_t iter;
            if (!navigateToField(fieldPath, iter))
            {
                throw DBException("D6B7F1DFE191", "Field not found: " + fieldPath, system_utils::captureCallStack());
            }

            if (!BSON_ITER_HOLDS_ARRAY(&iter))
            {
                throw DBException("20E5C450C426", "Field is not an array: " + fieldPath, system_utils::captureCallStack());
            }

            std::vector<std::shared_ptr<DocumentDBData>> result;

            bson_iter_t arrayIter;
            const uint8_t *data = nullptr;
            uint32_t length = 0;
            bson_iter_array(&iter, &length, &data);

            bson_t arrayBson;
            if (!bson_init_static(&arrayBson, data, length))
            {
                throw DBException("494F066BFAC9", "Failed to initialize array BSON", system_utils::captureCallStack());
            }

            if (bson_iter_init(&arrayIter, &arrayBson))
            {
                while (bson_iter_next(&arrayIter))
                {
                    if (BSON_ITER_HOLDS_DOCUMENT(&arrayIter))
                    {
                        const uint8_t *docData = nullptr;
                        uint32_t docLength = 0;
                        bson_iter_document(&arrayIter, &docLength, &docData);

                        bson_t *subdoc = bson_new_from_data(docData, docLength);
                        if (subdoc)
                        {
                            result.push_back(std::make_shared<MongoDBDocument>(subdoc));
                        }
                    }
                }
            }

            return result;
        }

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
                throw DBException("FB88E110970F", "Field is not an array: " + fieldPath, system_utils::captureCallStack());
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
            if (fieldPath.find('.') == std::string::npos)
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
                        if (std::strcmp(key, fieldPath.c_str()) != 0)
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
                throw DBException("EF1086B33F07", "Nested field setting not yet implemented: " + fieldPath, system_utils::captureCallStack());
            }
        }

        void MongoDBDocument::setInt(const std::string &fieldPath, int64_t value)
        {
            MONGODB_LOCK_GUARD(m_mutex);
            validateDocument();

            if (fieldPath.find('.') == std::string::npos)
            {
                bson_t *newBson = bson_new();
                if (!newBson)
                {
                    throw DBException("3CA7686125A7", "Failed to create new BSON document", system_utils::captureCallStack());
                }

                bson_iter_t iter;
                if (bson_iter_init(&iter, m_bson.get()))
                {
                    while (bson_iter_next(&iter))
                    {
                        const char *key = bson_iter_key(&iter);
                        if (std::strcmp(key, fieldPath.c_str()) != 0)
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

            if (fieldPath.find('.') == std::string::npos)
            {
                bson_t *newBson = bson_new();
                if (!newBson)
                {
                    throw DBException("1A96C0C78D87", "Failed to create new BSON document", system_utils::captureCallStack());
                }

                bson_iter_t iter;
                if (bson_iter_init(&iter, m_bson.get()))
                {
                    while (bson_iter_next(&iter))
                    {
                        const char *key = bson_iter_key(&iter);
                        if (std::strcmp(key, fieldPath.c_str()) != 0)
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
                throw DBException("1B37D38CE245", "Nested field setting not yet implemented: " + fieldPath, system_utils::captureCallStack());
            }
        }

        void MongoDBDocument::setBool(const std::string &fieldPath, bool value)
        {
            MONGODB_LOCK_GUARD(m_mutex);
            validateDocument();

            if (fieldPath.find('.') == std::string::npos)
            {
                bson_t *newBson = bson_new();
                if (!newBson)
                {
                    throw DBException("594AC0F375D5", "Failed to create new BSON document", system_utils::captureCallStack());
                }

                bson_iter_t iter;
                if (bson_iter_init(&iter, m_bson.get()))
                {
                    while (bson_iter_next(&iter))
                    {
                        const char *key = bson_iter_key(&iter);
                        if (std::strcmp(key, fieldPath.c_str()) != 0)
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

            if (fieldPath.find('.') == std::string::npos)
            {
                bson_t *newBson = bson_new();
                if (!newBson)
                {
                    throw DBException("E6657751DB88", "Failed to create new BSON document", system_utils::captureCallStack());
                }

                bson_iter_t iter;
                if (bson_iter_init(&iter, m_bson.get()))
                {
                    while (bson_iter_next(&iter))
                    {
                        const char *key = bson_iter_key(&iter);
                        if (std::strcmp(key, fieldPath.c_str()) != 0)
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
                throw DBException("418CA26C4282", "Nested field setting not yet implemented: " + fieldPath, system_utils::captureCallStack());
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

            if (fieldPath.find('.') == std::string::npos)
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
                        if (std::strcmp(key, fieldPath.c_str()) != 0)
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

            if (fieldPath.find('.') == std::string::npos)
            {
                bson_t *newBson = bson_new();
                if (!newBson)
                {
                    throw DBException("19B7509217A9", "Failed to create new BSON document", system_utils::captureCallStack());
                }

                bson_iter_t iter;
                if (bson_iter_init(&iter, m_bson.get()))
                {
                    while (bson_iter_next(&iter))
                    {
                        const char *key = bson_iter_key(&iter);
                        if (std::strcmp(key, fieldPath.c_str()) != 0)
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
                throw DBException("18C5E7864E82", "Nested field setting not yet implemented: " + fieldPath, system_utils::captureCallStack());
            }
        }

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

            if (fieldPath.find('.') != std::string::npos)
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
                    if (std::strcmp(key, fieldPath.c_str()) != 0)
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

            m_bson.reset(bson_new());
            if (!m_bson)
            {
                throw DBException("1672D32248D8", "Failed to create empty BSON document", system_utils::captureCallStack());
            }

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

        // This is a continuation file - will be merged into driver_mongodb.cpp

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

        // ============================================================================
        // MongoDBCursor Implementation
        // ============================================================================

        MongoDBCursor::MongoDBCursor(std::weak_ptr<mongoc_client_t> client, mongoc_cursor_t *cursor)
            : m_client(std::move(client)),
              m_cursor(cursor)
        {
            MONGODB_DEBUG("MongoDBCursor::constructor - Creating cursor");
            if (!m_cursor)
            {
                throw DBException("C9D5E4F3A7B8", "Cannot create cursor from null pointer", system_utils::captureCallStack());
            }
            MONGODB_DEBUG("MongoDBCursor::constructor - Done");
        }

        MongoDBCursor::~MongoDBCursor()
        {
            MONGODB_DEBUG("MongoDBCursor::destructor - Destroying cursor");
        }

        MongoDBCursor::MongoDBCursor(MongoDBCursor &&other) noexcept
            : m_client(std::move(other.m_client)),
              m_cursor(std::move(other.m_cursor)),
              m_currentDoc(std::move(other.m_currentDoc)),
              m_position(other.m_position),
              m_iterationStarted(other.m_iterationStarted),
              m_exhausted(other.m_exhausted)
        {
            other.m_exhausted = true;
        }

        MongoDBCursor &MongoDBCursor::operator=(MongoDBCursor &&other) noexcept
        {
            if (this != &other)
            {
                m_client = std::move(other.m_client);
                m_cursor = std::move(other.m_cursor);
                m_currentDoc = std::move(other.m_currentDoc);
                m_position = other.m_position;
                m_iterationStarted = other.m_iterationStarted;
                m_exhausted = other.m_exhausted;
                other.m_exhausted = true;
            }
            return *this;
        }

        void MongoDBCursor::validateConnection() const
        {
            if (m_client.expired())
            {
                throw DBException("D0E6F5A4B9C8", "MongoDB connection has been closed", system_utils::captureCallStack());
            }
        }

        void MongoDBCursor::validateCursor() const
        {
            if (!m_cursor)
            {
                throw DBException("E1F7A6B5C0D9", "Cursor is not initialized or has been closed", system_utils::captureCallStack());
            }
        }

        mongoc_client_t *MongoDBCursor::getClient() const
        {
            auto client = m_client.lock();
            if (!client)
            {
                throw DBException("F2A8B7C6D1E0", "MongoDB connection has been closed", system_utils::captureCallStack());
            }
            return client.get();
        }

        void MongoDBCursor::close()
        {
            MONGODB_DEBUG("MongoDBCursor::close - Closing cursor");
            MONGODB_LOCK_GUARD(m_mutex);
            m_cursor.reset();
            m_currentDoc.reset();
            m_exhausted = true;
            MONGODB_DEBUG("MongoDBCursor::close - Done");
        }

        bool MongoDBCursor::isEmpty()
        {
            MONGODB_LOCK_GUARD(m_mutex);
            validateCursor();
            if (!m_iterationStarted)
            {
                return !hasNext();
            }
            return m_exhausted && !m_currentDoc;
        }

        bool MongoDBCursor::next()
        {
            MONGODB_DEBUG("MongoDBCursor::next - Moving to next document, position: " << m_position);
            MONGODB_LOCK_GUARD(m_mutex);
            validateConnection();
            validateCursor();

            m_iterationStarted = true;

            const bson_t *doc = nullptr;
            if (mongoc_cursor_next(m_cursor.get(), &doc))
            {
                bson_t *docCopy = bson_copy(doc);
                if (!docCopy)
                {
                    throw DBException("A3B9C8D7E2F1", "Failed to copy document from cursor", system_utils::captureCallStack());
                }
                m_currentDoc = std::make_shared<MongoDBDocument>(docCopy);
                m_position++;
                MONGODB_DEBUG("MongoDBCursor::next - Found document at position: " << m_position);
                return true;
            }

            bson_error_t error;
            if (mongoc_cursor_error(m_cursor.get(), &error))
            {
                throw DBException("B4C0D9E8F3A2", std::string("Cursor error: ") + error.message, system_utils::captureCallStack());
            }

            MONGODB_DEBUG("MongoDBCursor::next - Cursor exhausted");
            m_exhausted = true;
            m_currentDoc.reset();
            return false;
        }

        bool MongoDBCursor::hasNext()
        {
            MONGODB_LOCK_GUARD(m_mutex);
            validateConnection();
            validateCursor();
            if (m_exhausted)
                return false;
            return mongoc_cursor_more(m_cursor.get());
        }

        std::shared_ptr<DocumentDBData> MongoDBCursor::current()
        {
            MONGODB_LOCK_GUARD(m_mutex);
            validateCursor();
            if (!m_currentDoc)
            {
                throw DBException("C5D1E0F9A4B3", "No current document - call next() first", system_utils::captureCallStack());
            }
            return m_currentDoc;
        }

        std::shared_ptr<DocumentDBData> MongoDBCursor::nextDocument()
        {
            if (!next())
            {
                throw DBException("D6E2F1A0B5C4", "No more documents in cursor", system_utils::captureCallStack());
            }
            return m_currentDoc;
        }

        std::vector<std::shared_ptr<DocumentDBData>> MongoDBCursor::toVector()
        {
            MONGODB_LOCK_GUARD(m_mutex);
            validateConnection();
            validateCursor();

            std::vector<std::shared_ptr<DocumentDBData>> result;
            const bson_t *doc = nullptr;
            while (mongoc_cursor_next(m_cursor.get(), &doc))
            {
                bson_t *docCopy = bson_copy(doc);
                if (docCopy)
                {
                    result.push_back(std::make_shared<MongoDBDocument>(docCopy));
                }
            }

            bson_error_t error;
            if (mongoc_cursor_error(m_cursor.get(), &error))
            {
                throw DBException("E7F3A2B1C6D5", std::string("Cursor error: ") + error.message, system_utils::captureCallStack());
            }

            m_exhausted = true;
            m_currentDoc.reset();
            return result;
        }

        std::vector<std::shared_ptr<DocumentDBData>> MongoDBCursor::getBatch(size_t batchSize)
        {
            MONGODB_LOCK_GUARD(m_mutex);
            validateConnection();
            validateCursor();

            std::vector<std::shared_ptr<DocumentDBData>> result;
            result.reserve(batchSize);

            const bson_t *doc = nullptr;
            size_t count = 0;

            while (count < batchSize && mongoc_cursor_next(m_cursor.get(), &doc))
            {
                bson_t *docCopy = bson_copy(doc);
                if (docCopy)
                {
                    result.push_back(std::make_shared<MongoDBDocument>(docCopy));
                    count++;
                    m_position++;
                }
            }

            bson_error_t error;
            if (mongoc_cursor_error(m_cursor.get(), &error))
            {
                throw DBException("F8A4B3C2D7E6", std::string("Cursor error: ") + error.message, system_utils::captureCallStack());
            }

            if (count < batchSize)
                m_exhausted = true;
            return result;
        }

        int64_t MongoDBCursor::count() { return -1; }
        uint64_t MongoDBCursor::getPosition() { return m_position; }

        DocumentDBCursor &MongoDBCursor::skip(uint64_t n)
        {
            MONGODB_LOCK_GUARD(m_mutex);
            if (m_iterationStarted)
            {
                throw DBException("A9B5C4D3E8F7", "Cannot modify cursor after iteration has begun", system_utils::captureCallStack());
            }
            m_skipCount = n;
            return *this;
        }

        DocumentDBCursor &MongoDBCursor::limit(uint64_t n)
        {
            MONGODB_LOCK_GUARD(m_mutex);
            if (m_iterationStarted)
            {
                throw DBException("B0C6D5E4F9A8", "Cannot modify cursor after iteration has begun", system_utils::captureCallStack());
            }
            m_limitCount = n;
            return *this;
        }

        DocumentDBCursor &MongoDBCursor::sort(const std::string &fieldPath, bool ascending)
        {
            MONGODB_LOCK_GUARD(m_mutex);
            if (m_iterationStarted)
            {
                throw DBException("C1D7E6F5A0B9", "Cannot modify cursor after iteration has begun", system_utils::captureCallStack());
            }
            std::ostringstream oss;
            oss << "{\"" << fieldPath << "\": " << (ascending ? "1" : "-1") << "}";
            m_sortSpec = oss.str();
            return *this;
        }

        bool MongoDBCursor::isExhausted() { return m_exhausted; }

        void MongoDBCursor::rewind()
        {
            throw DBException("D2E8F7A6B1C0", "MongoDB cursors do not support rewinding", system_utils::captureCallStack());
        }

        bool MongoDBCursor::isConnectionValid() const { return !m_client.expired(); }

        std::string MongoDBCursor::getError() const
        {
            MONGODB_LOCK_GUARD(m_mutex);
            if (!m_cursor)
                return "";
            bson_error_t error;
            if (mongoc_cursor_error(m_cursor.get(), &error))
            {
                return error.message;
            }
            return "";
        }

        // ============================================================================
        // MongoDBCollection Implementation
        // ============================================================================

        MongoDBCollection::MongoDBCollection(std::weak_ptr<mongoc_client_t> client,
                                             mongoc_collection_t *collection,
                                             const std::string &name,
                                             const std::string &databaseName)
            : m_client(std::move(client)),
              m_collection(collection),
              m_name(name),
              m_databaseName(databaseName)
        {
            MONGODB_DEBUG("MongoDBCollection::constructor - Creating collection: " << name << " in database: " << databaseName);
            if (!m_collection)
            {
                throw DBException("E3F9A8B7C2D1", "Cannot create collection from null pointer", system_utils::captureCallStack());
            }
            MONGODB_DEBUG("MongoDBCollection::constructor - Done");
        }

        MongoDBCollection::MongoDBCollection(MongoDBCollection &&other) noexcept
            : m_client(std::move(other.m_client)),
              m_collection(std::move(other.m_collection)),
              m_name(std::move(other.m_name)),
              m_databaseName(std::move(other.m_databaseName)) {}

        MongoDBCollection &MongoDBCollection::operator=(MongoDBCollection &&other) noexcept
        {
            if (this != &other)
            {
                m_client = std::move(other.m_client);
                m_collection = std::move(other.m_collection);
                m_name = std::move(other.m_name);
                m_databaseName = std::move(other.m_databaseName);
            }
            return *this;
        }

        void MongoDBCollection::validateConnection() const
        {
            if (m_client.expired())
            {
                throw DBException("F4A0B9C8D3E2", "MongoDB connection has been closed", system_utils::captureCallStack());
            }
        }

        mongoc_client_t *MongoDBCollection::getClient() const
        {
            auto client = m_client.lock();
            if (!client)
            {
                throw DBException("A5B1C0D9E4F3", "MongoDB connection has been closed", system_utils::captureCallStack());
            }
            return client.get();
        }

        BsonHandle MongoDBCollection::parseFilter(const std::string &filter) const
        {
            if (filter.empty())
                return makeBsonHandle();
            return makeBsonHandleFromJson(filter);
        }

        void MongoDBCollection::throwMongoError(const bson_error_t &error, const std::string &operation) const
        {
            throw DBException("B6C2D1E0F5A4", operation + " failed: " + std::string(error.message), system_utils::captureCallStack());
        }

        std::string MongoDBCollection::getName() const { return m_name; }
        std::string MongoDBCollection::getNamespace() const { return m_databaseName + "." + m_name; }

        uint64_t MongoDBCollection::estimatedDocumentCount()
        {
            MONGODB_LOCK_GUARD(m_mutex);
            validateConnection();
            bson_error_t error;
            int64_t count = mongoc_collection_estimated_document_count(
                m_collection.get(), nullptr, nullptr, nullptr, &error);
            if (count < 0)
                throwMongoError(error, "estimatedDocumentCount");
            return static_cast<uint64_t>(count);
        }

        uint64_t MongoDBCollection::countDocuments(const std::string &filter)
        {
            MONGODB_LOCK_GUARD(m_mutex);
            validateConnection();
            BsonHandle filterBson = parseFilter(filter);
            bson_error_t error;
            int64_t count = mongoc_collection_count_documents(
                m_collection.get(), filterBson.get(), nullptr, nullptr, nullptr, &error);
            if (count < 0)
                throwMongoError(error, "countDocuments");
            return static_cast<uint64_t>(count);
        }

        DocumentInsertResult MongoDBCollection::insertOne(
            std::shared_ptr<DocumentDBData> document,
            const DocumentWriteOptions &options)
        {
            MONGODB_DEBUG("MongoDBCollection::insertOne - Inserting document into: " << m_name);
            MONGODB_LOCK_GUARD(m_mutex);
            validateConnection();

            auto mongoDoc = std::dynamic_pointer_cast<MongoDBDocument>(document);
            if (!mongoDoc)
            {
                throw DBException("C7D3E2F1A6B5", "Document must be a MongoDBDocument", system_utils::captureCallStack());
            }

            bson_t *bson = mongoDoc->getBsonMutable();
            bson_iter_t iter;
            if (!bson_iter_init_find(&iter, bson, "_id"))
            {
                bson_oid_t oid;
                bson_oid_init(&oid, nullptr);
                BSON_APPEND_OID(bson, "_id", &oid);
            }

            bson_error_t error;
            bson_t reply;
            bson_init(&reply);

            bool success = mongoc_collection_insert_one(
                m_collection.get(), bson, nullptr, &reply, &error);

            DocumentInsertResult result;
            result.acknowledged = success;

            if (success)
            {
                result.insertedId = mongoDoc->getId();
                result.insertedCount = 1;
            }
            else
            {
                bson_destroy(&reply);
                if (options.ordered)
                    throwMongoError(error, "insertOne");
                result.insertedCount = 0;
            }

            bson_destroy(&reply);
            return result;
        }

        DocumentInsertResult MongoDBCollection::insertOne(
            const std::string &jsonDocument,
            const DocumentWriteOptions &options)
        {
            auto doc = std::make_shared<MongoDBDocument>(jsonDocument);
            return insertOne(doc, options);
        }

        DocumentInsertResult MongoDBCollection::insertMany(
            const std::vector<std::shared_ptr<DocumentDBData>> &documents,
            const DocumentWriteOptions &options)
        {
            MONGODB_LOCK_GUARD(m_mutex);
            validateConnection();

            if (documents.empty())
            {
                DocumentInsertResult result;
                result.acknowledged = true;
                result.insertedCount = 0;
                return result;
            }

            // Suppress -Wignored-attributes warning for bson_t pointer vector
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wignored-attributes"
#endif
            std::vector<const bson_t *> bsonDocs;
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
            bsonDocs.reserve(documents.size());

            for (const auto &doc : documents)
            {
                auto mongoDoc = std::dynamic_pointer_cast<MongoDBDocument>(doc);
                if (!mongoDoc)
                {
                    throw DBException("D8E4F3A2B7C6", "All documents must be MongoDBDocument instances", system_utils::captureCallStack());
                }

                bson_t *bson = mongoDoc->getBsonMutable();
                bson_iter_t iter;
                if (!bson_iter_init_find(&iter, bson, "_id"))
                {
                    bson_oid_t oid;
                    bson_oid_init(&oid, nullptr);
                    BSON_APPEND_OID(bson, "_id", &oid);
                }
                bsonDocs.push_back(mongoDoc->getBson());
            }

            bson_t opts = BSON_INITIALIZER;
            BSON_APPEND_BOOL(&opts, "ordered", options.ordered);
            if (options.bypassValidation)
            {
                BSON_APPEND_BOOL(&opts, "bypassDocumentValidation", true);
            }

            bson_error_t error;
            bson_t reply;
            bson_init(&reply);

            bool success = mongoc_collection_insert_many(
                m_collection.get(), bsonDocs.data(), bsonDocs.size(), &opts, &reply, &error);

            bson_destroy(&opts);

            DocumentInsertResult result;
            result.acknowledged = success;

            if (success)
            {
                result.insertedCount = documents.size();
                for (const auto &doc : documents)
                {
                    auto mongoDoc = std::dynamic_pointer_cast<MongoDBDocument>(doc);
                    if (mongoDoc)
                        result.insertedIds.push_back(mongoDoc->getId());
                }
            }
            else
            {
                bson_destroy(&reply);
                throwMongoError(error, "insertMany");
            }

            bson_destroy(&reply);
            return result;
        }

        std::shared_ptr<DocumentDBData> MongoDBCollection::findOne(const std::string &filter)
        {
            MONGODB_LOCK_GUARD(m_mutex);
            validateConnection();

            BsonHandle filterBson = parseFilter(filter);
            bson_t opts = BSON_INITIALIZER;
            BSON_APPEND_INT64(&opts, "limit", 1);

            mongoc_cursor_t *cursor = mongoc_collection_find_with_opts(
                m_collection.get(), filterBson.get(), &opts, nullptr);
            bson_destroy(&opts);

            if (!cursor)
            {
                throw DBException("E9F5A4B3C8D7", "Failed to create cursor for findOne", system_utils::captureCallStack());
            }

            const bson_t *doc = nullptr;
            std::shared_ptr<DocumentDBData> result;

            if (mongoc_cursor_next(cursor, &doc))
            {
                bson_t *docCopy = bson_copy(doc);
                if (docCopy)
                    result = std::make_shared<MongoDBDocument>(docCopy);
            }

            bson_error_t error;
            if (mongoc_cursor_error(cursor, &error))
            {
                mongoc_cursor_destroy(cursor);
                throw DBException("F0A6B5C4D9E8", std::string("findOne error: ") + error.message, system_utils::captureCallStack());
            }

            mongoc_cursor_destroy(cursor);
            return result;
        }

        std::shared_ptr<DocumentDBData> MongoDBCollection::findById(const std::string &id)
        {
            std::string filter;
            if (bson_oid_is_valid(id.c_str(), id.length()))
            {
                filter = "{\"_id\": {\"$oid\": \"" + id + "\"}}";
            }
            else
            {
                filter = "{\"_id\": \"" + id + "\"}";
            }
            return findOne(filter);
        }

        std::shared_ptr<DocumentDBCursor> MongoDBCollection::find(const std::string &filter)
        {
            MONGODB_LOCK_GUARD(m_mutex);
            validateConnection();

            BsonHandle filterBson = parseFilter(filter);
            mongoc_cursor_t *cursor = mongoc_collection_find_with_opts(
                m_collection.get(), filterBson.get(), nullptr, nullptr);

            if (!cursor)
            {
                throw DBException("A1B7C6D5E0F9", "Failed to create cursor for find", system_utils::captureCallStack());
            }

            return std::make_shared<MongoDBCursor>(m_client, cursor);
        }

        std::shared_ptr<DocumentDBCursor> MongoDBCollection::find(
            const std::string &filter,
            const std::string &projection)
        {
            MONGODB_LOCK_GUARD(m_mutex);
            validateConnection();

            BsonHandle filterBson = parseFilter(filter);
            BsonHandle projBson = parseFilter(projection);

            bson_t opts = BSON_INITIALIZER;
            BSON_APPEND_DOCUMENT(&opts, "projection", projBson.get());

            mongoc_cursor_t *cursor = mongoc_collection_find_with_opts(
                m_collection.get(), filterBson.get(), &opts, nullptr);
            bson_destroy(&opts);

            if (!cursor)
            {
                throw DBException("B2C8D7E6F1A0", "Failed to create cursor for find with projection", system_utils::captureCallStack());
            }

            return std::make_shared<MongoDBCursor>(m_client, cursor);
        }

        DocumentUpdateResult MongoDBCollection::updateOne(
            const std::string &filter,
            const std::string &update,
            const DocumentUpdateOptions &options)
        {
            MONGODB_LOCK_GUARD(m_mutex);
            validateConnection();

            BsonHandle filterBson = parseFilter(filter);
            BsonHandle updateBson = makeBsonHandleFromJson(update);

            bson_t opts = BSON_INITIALIZER;
            if (options.upsert)
                BSON_APPEND_BOOL(&opts, "upsert", true);

            bson_error_t error;
            bson_t reply;
            bson_init(&reply);

            bool success = mongoc_collection_update_one(
                m_collection.get(), filterBson.get(), updateBson.get(), &opts, &reply, &error);
            bson_destroy(&opts);

            DocumentUpdateResult result;
            result.acknowledged = success;

            if (success)
            {
                bson_iter_t iter;
                if (bson_iter_init_find(&iter, &reply, "matchedCount"))
                    result.matchedCount = static_cast<uint64_t>(bson_iter_as_int64(&iter));
                if (bson_iter_init_find(&iter, &reply, "modifiedCount"))
                    result.modifiedCount = static_cast<uint64_t>(bson_iter_as_int64(&iter));
                if (bson_iter_init_find(&iter, &reply, "upsertedId") && BSON_ITER_HOLDS_OID(&iter))
                {
                    const bson_oid_t *oid = bson_iter_oid(&iter);
                    char oidStr[25];
                    bson_oid_to_string(oid, oidStr);
                    result.upsertedId = oidStr;
                }
            }
            else
            {
                bson_destroy(&reply);
                throwMongoError(error, "updateOne");
            }

            bson_destroy(&reply);
            return result;
        }

        DocumentUpdateResult MongoDBCollection::updateMany(
            const std::string &filter,
            const std::string &update,
            const DocumentUpdateOptions &options)
        {
            MONGODB_LOCK_GUARD(m_mutex);
            validateConnection();

            BsonHandle filterBson = parseFilter(filter);
            BsonHandle updateBson = makeBsonHandleFromJson(update);

            bson_t opts = BSON_INITIALIZER;
            if (options.upsert)
                BSON_APPEND_BOOL(&opts, "upsert", true);

            bson_error_t error;
            bson_t reply;
            bson_init(&reply);

            bool success = mongoc_collection_update_many(
                m_collection.get(), filterBson.get(), updateBson.get(), &opts, &reply, &error);
            bson_destroy(&opts);

            DocumentUpdateResult result;
            result.acknowledged = success;

            if (success)
            {
                bson_iter_t iter;
                if (bson_iter_init_find(&iter, &reply, "matchedCount"))
                    result.matchedCount = static_cast<uint64_t>(bson_iter_as_int64(&iter));
                if (bson_iter_init_find(&iter, &reply, "modifiedCount"))
                    result.modifiedCount = static_cast<uint64_t>(bson_iter_as_int64(&iter));
            }
            else
            {
                bson_destroy(&reply);
                throwMongoError(error, "updateMany");
            }

            bson_destroy(&reply);
            return result;
        }

        DocumentUpdateResult MongoDBCollection::replaceOne(
            const std::string &filter,
            std::shared_ptr<DocumentDBData> replacement,
            const DocumentUpdateOptions &options)
        {
            MONGODB_LOCK_GUARD(m_mutex);
            validateConnection();

            auto mongoDoc = std::dynamic_pointer_cast<MongoDBDocument>(replacement);
            if (!mongoDoc)
            {
                throw DBException("C3D9E8F7A2B1", "Replacement must be a MongoDBDocument", system_utils::captureCallStack());
            }

            BsonHandle filterBson = parseFilter(filter);

            bson_t opts = BSON_INITIALIZER;
            if (options.upsert)
                BSON_APPEND_BOOL(&opts, "upsert", true);

            bson_error_t error;
            bson_t reply;
            bson_init(&reply);

            bool success = mongoc_collection_replace_one(
                m_collection.get(), filterBson.get(), mongoDoc->getBson(), &opts, &reply, &error);
            bson_destroy(&opts);

            DocumentUpdateResult result;
            result.acknowledged = success;

            if (success)
            {
                bson_iter_t iter;
                if (bson_iter_init_find(&iter, &reply, "matchedCount"))
                    result.matchedCount = static_cast<uint64_t>(bson_iter_as_int64(&iter));
                if (bson_iter_init_find(&iter, &reply, "modifiedCount"))
                    result.modifiedCount = static_cast<uint64_t>(bson_iter_as_int64(&iter));
            }
            else
            {
                bson_destroy(&reply);
                throwMongoError(error, "replaceOne");
            }

            bson_destroy(&reply);
            return result;
        }

        DocumentDeleteResult MongoDBCollection::deleteOne(const std::string &filter)
        {
            MONGODB_LOCK_GUARD(m_mutex);
            validateConnection();

            BsonHandle filterBson = parseFilter(filter);

            bson_error_t error;
            bson_t reply;
            bson_init(&reply);

            bool success = mongoc_collection_delete_one(
                m_collection.get(), filterBson.get(), nullptr, &reply, &error);

            DocumentDeleteResult result;
            result.acknowledged = success;

            if (success)
            {
                bson_iter_t iter;
                if (bson_iter_init_find(&iter, &reply, "deletedCount"))
                    result.deletedCount = static_cast<uint64_t>(bson_iter_as_int64(&iter));
            }
            else
            {
                bson_destroy(&reply);
                throwMongoError(error, "deleteOne");
            }

            bson_destroy(&reply);
            return result;
        }

        DocumentDeleteResult MongoDBCollection::deleteMany(const std::string &filter)
        {
            MONGODB_LOCK_GUARD(m_mutex);
            validateConnection();

            BsonHandle filterBson = parseFilter(filter);

            bson_error_t error;
            bson_t reply;
            bson_init(&reply);

            bool success = mongoc_collection_delete_many(
                m_collection.get(), filterBson.get(), nullptr, &reply, &error);

            DocumentDeleteResult result;
            result.acknowledged = success;

            if (success)
            {
                bson_iter_t iter;
                if (bson_iter_init_find(&iter, &reply, "deletedCount"))
                    result.deletedCount = static_cast<uint64_t>(bson_iter_as_int64(&iter));
            }
            else
            {
                bson_destroy(&reply);
                throwMongoError(error, "deleteMany");
            }

            bson_destroy(&reply);
            return result;
        }

        DocumentDeleteResult MongoDBCollection::deleteById(const std::string &id)
        {
            std::string filter;
            if (bson_oid_is_valid(id.c_str(), id.length()))
            {
                filter = "{\"_id\": {\"$oid\": \"" + id + "\"}}";
            }
            else
            {
                filter = "{\"_id\": \"" + id + "\"}";
            }
            return deleteOne(filter);
        }

        std::string MongoDBCollection::createIndex(const std::string &keys, const std::string &options)
        {
            MONGODB_LOCK_GUARD(m_mutex);
            validateConnection();

            BsonHandle keysBson = makeBsonHandleFromJson(keys);

            // Parse options if provided
            mongoc_index_opt_t indexOpts;
            mongoc_index_opt_init(&indexOpts);

            bool isUnique = false;
            bool isSparse = false;
            std::string indexName;

            if (!options.empty())
            {
                bson_error_t parseError;
                bson_t *optsBson = bson_new_from_json(
                    reinterpret_cast<const uint8_t *>(options.c_str()),
                    static_cast<ssize_t>(options.length()),
                    &parseError);

                if (optsBson)
                {
                    bson_iter_t iter;
                    if (bson_iter_init_find(&iter, optsBson, "unique") && BSON_ITER_HOLDS_BOOL(&iter))
                    {
                        isUnique = bson_iter_bool(&iter);
                    }
                    if (bson_iter_init_find(&iter, optsBson, "sparse") && BSON_ITER_HOLDS_BOOL(&iter))
                    {
                        isSparse = bson_iter_bool(&iter);
                    }
                    if (bson_iter_init_find(&iter, optsBson, "name") && BSON_ITER_HOLDS_UTF8(&iter))
                    {
                        uint32_t len = 0;
                        const char *name = bson_iter_utf8(&iter, &len);
                        indexName = std::string(name, len);
                    }
                    bson_destroy(optsBson);
                }
            }

            indexOpts.unique = isUnique;
            indexOpts.sparse = isSparse;
            if (!indexName.empty())
            {
                indexOpts.name = indexName.c_str();
            }

            // Generate index name from keys if not provided
            char *generatedName = mongoc_collection_keys_to_index_string(keysBson.get());
            std::string result = indexName.empty() ? (generatedName ? generatedName : "") : indexName;
            bson_free(generatedName);

            bson_error_t error;
            
            // Suppress deprecation warning for mongoc_collection_create_index
            // We use this older API for broader compatibility with different libmongoc versions
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
            bool success = mongoc_collection_create_index(
                m_collection.get(), keysBson.get(), &indexOpts, &error);
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

            if (!success)
            {
                throwMongoError(error, "createIndex");
            }

            return result;
        }

        void MongoDBCollection::dropIndex(const std::string &indexName)
        {
            MONGODB_LOCK_GUARD(m_mutex);
            validateConnection();

            bson_error_t error;
            bool success = mongoc_collection_drop_index(m_collection.get(), indexName.c_str(), &error);

            if (!success)
            {
                throwMongoError(error, "dropIndex");
            }
        }

        void MongoDBCollection::dropAllIndexes()
        {
            MONGODB_LOCK_GUARD(m_mutex);
            validateConnection();

            // Drop all indexes except _id
            bson_t cmd = BSON_INITIALIZER;
            BSON_APPEND_UTF8(&cmd, "dropIndexes", m_name.c_str());
            BSON_APPEND_UTF8(&cmd, "index", "*");

            bson_error_t error;
            bson_t reply;
            bson_init(&reply);

            auto client = getClient();
            MongoDatabaseHandle db(mongoc_client_get_database(client, m_databaseName.c_str()));

            bool success = mongoc_database_command_simple(db.get(), &cmd, nullptr, &reply, &error);

            bson_destroy(&cmd);
            bson_destroy(&reply);

            if (!success)
            {
                throwMongoError(error, "dropAllIndexes");
            }
        }

        std::vector<std::string> MongoDBCollection::listIndexes()
        {
            MONGODB_LOCK_GUARD(m_mutex);
            validateConnection();

            std::vector<std::string> result;

            mongoc_cursor_t *cursor = mongoc_collection_find_indexes_with_opts(
                m_collection.get(), nullptr);

            if (!cursor)
            {
                throw DBException("G1H2I3J4K5L6", "Failed to list indexes", system_utils::captureCallStack());
            }

            const bson_t *doc = nullptr;
            while (mongoc_cursor_next(cursor, &doc))
            {
                size_t length = 0;
                char *json = bson_as_relaxed_extended_json(doc, &length);
                if (json)
                {
                    result.emplace_back(json, length);
                    bson_free(json);
                }
            }

            bson_error_t error;
            if (mongoc_cursor_error(cursor, &error))
            {
                mongoc_cursor_destroy(cursor);
                throw DBException("H2I3J4K5L6M7", std::string("listIndexes error: ") + error.message, system_utils::captureCallStack());
            }

            mongoc_cursor_destroy(cursor);
            return result;
        }

        void MongoDBCollection::drop()
        {
            MONGODB_LOCK_GUARD(m_mutex);
            validateConnection();

            bson_error_t error;
            bool success = mongoc_collection_drop(m_collection.get(), &error);

            if (!success)
            {
                throwMongoError(error, "drop");
            }
        }

        void MongoDBCollection::rename(const std::string &newName, bool dropTarget)
        {
            MONGODB_LOCK_GUARD(m_mutex);
            validateConnection();

            bson_error_t error;
            bool success = mongoc_collection_rename(
                m_collection.get(), m_databaseName.c_str(), newName.c_str(), dropTarget, &error);

            if (!success)
            {
                throwMongoError(error, "rename");
            }

            m_name = newName;
        }

        std::shared_ptr<DocumentDBCursor> MongoDBCollection::aggregate(const std::string &pipeline)
        {
            MONGODB_LOCK_GUARD(m_mutex);
            validateConnection();

            BsonHandle pipelineBson = makeBsonHandleFromJson(pipeline);

            mongoc_cursor_t *cursor = mongoc_collection_aggregate(
                m_collection.get(), MONGOC_QUERY_NONE, pipelineBson.get(), nullptr, nullptr);

            if (!cursor)
            {
                throw DBException("I3J4K5L6M7N8", "Failed to create cursor for aggregate", system_utils::captureCallStack());
            }

            return std::make_shared<MongoDBCursor>(m_client, cursor);
        }

        std::vector<std::string> MongoDBCollection::distinct(
            const std::string &fieldPath,
            const std::string &filter)
        {
            MONGODB_LOCK_GUARD(m_mutex);
            validateConnection();

            std::vector<std::string> result;

            bson_t cmd = BSON_INITIALIZER;
            BSON_APPEND_UTF8(&cmd, "distinct", m_name.c_str());
            BSON_APPEND_UTF8(&cmd, "key", fieldPath.c_str());

            if (!filter.empty())
            {
                BsonHandle filterBson = parseFilter(filter);
                BSON_APPEND_DOCUMENT(&cmd, "query", filterBson.get());
            }

            bson_error_t error;
            bson_t reply;
            bson_init(&reply);

            auto client = getClient();
            MongoDatabaseHandle db(mongoc_client_get_database(client, m_databaseName.c_str()));

            bool success = mongoc_database_command_simple(db.get(), &cmd, nullptr, &reply, &error);

            bson_destroy(&cmd);

            if (!success)
            {
                bson_destroy(&reply);
                throwMongoError(error, "distinct");
            }

            // Extract values from reply
            bson_iter_t iter;
            if (bson_iter_init_find(&iter, &reply, "values") && BSON_ITER_HOLDS_ARRAY(&iter))
            {
                bson_iter_t arrayIter;
                const uint8_t *data = nullptr;
                uint32_t length = 0;
                bson_iter_array(&iter, &length, &data);

                bson_t arrayBson;
                if (bson_init_static(&arrayBson, data, length) && bson_iter_init(&arrayIter, &arrayBson))
                {
                    while (bson_iter_next(&arrayIter))
                    {
                        if (BSON_ITER_HOLDS_UTF8(&arrayIter))
                        {
                            uint32_t strLen = 0;
                            const char *str = bson_iter_utf8(&arrayIter, &strLen);
                            result.emplace_back(str, strLen);
                        }
                        else
                        {
                            // Convert other types to JSON string
                            bson_t tempDoc = BSON_INITIALIZER;
                            bson_append_iter(&tempDoc, "v", 1, &arrayIter);
                            size_t jsonLen = 0;
                            char *json = bson_as_relaxed_extended_json(&tempDoc, &jsonLen);
                            if (json)
                            {
                                result.emplace_back(json, jsonLen);
                                bson_free(json);
                            }
                            bson_destroy(&tempDoc);
                        }
                    }
                }
            }

            bson_destroy(&reply);
            return result;
        }

        bool MongoDBCollection::isConnectionValid() const
        {
            return !m_client.expired();
        }

        // ============================================================================
        // MongoDBConnection Implementation
        // ============================================================================

        MongoDBConnection::MongoDBConnection(const std::string &uri,
                                             const std::string &user,
                                             const std::string &password,
                                             const std::map<std::string, std::string> &options)
            : m_url(uri)
        {
            MONGODB_DEBUG("MongoDBConnection::constructor - Connecting to: " << uri);
            // Build the connection URI with credentials if provided
            std::string connectionUri = uri;

            // If user/password provided and not in URI, add them
            if (!user.empty() && uri.find("@") == std::string::npos)
            {
                // Parse the URI to insert credentials
                size_t schemeEnd = uri.find("://");
                if (schemeEnd != std::string::npos)
                {
                    std::string scheme = uri.substr(0, schemeEnd + 3);
                    std::string rest = uri.substr(schemeEnd + 3);
                    connectionUri = scheme + user + ":" + password + "@" + rest;
                }
            }

            // Add options to URI if provided
            if (!options.empty())
            {
                bool hasQuery = connectionUri.find('?') != std::string::npos;
                for (const auto &opt : options)
                {
                    connectionUri += (hasQuery ? "&" : "?");
                    connectionUri += opt.first + "=" + opt.second;
                    hasQuery = true;
                }
            }

            // Parse and validate URI
            bson_error_t error;
            MongoUriHandle mongoUri(mongoc_uri_new_with_error(connectionUri.c_str(), &error));

            if (!mongoUri)
            {
                throw DBException("J4K5L6M7N8O9", std::string("Invalid MongoDB URI: ") + error.message, system_utils::captureCallStack());
            }

            // Extract database name from URI
            const char *dbName = mongoc_uri_get_database(mongoUri.get());
            if (dbName)
            {
                m_databaseName = dbName;
            }

            // Create client
            mongoc_client_t *rawClient = mongoc_client_new_from_uri(mongoUri.get());
            if (!rawClient)
            {
                throw DBException("K5L6M7N8O9P0", "Failed to create MongoDB client", system_utils::captureCallStack());
            }

            // Set application name for monitoring
            mongoc_client_set_appname(rawClient, "cpp_dbc");

            // Wrap in shared_ptr with custom deleter
            m_client = MongoClientHandle(rawClient, MongoClientDeleter());

            // Test connection with ping
            bson_t pingCmd = BSON_INITIALIZER;
            BSON_APPEND_INT32(&pingCmd, "ping", 1);

            bson_t reply;
            bson_init(&reply);

            MongoDatabaseHandle adminDb(mongoc_client_get_database(m_client.get(), "admin"));

            bool pingSuccess = mongoc_database_command_simple(
                adminDb.get(), &pingCmd, nullptr, &reply, &error);

            bson_destroy(&pingCmd);
            bson_destroy(&reply);

            if (!pingSuccess)
            {
                m_client.reset();
                throw DBException("L6M7N8O9P0Q1", std::string("Failed to connect to MongoDB: ") + error.message, system_utils::captureCallStack());
            }

            m_closed = false;
            MONGODB_DEBUG("MongoDBConnection::constructor - Connected successfully");
        }

        MongoDBConnection::~MongoDBConnection()
        {
            MONGODB_DEBUG("MongoDBConnection::destructor - Destroying connection");
            if (!m_closed)
            {
                try
                {
                    close();
                }
                catch (...)
                {
                    // Suppress exceptions in destructor
                }
            }
            MONGODB_DEBUG("MongoDBConnection::destructor - Done");
        }

        MongoDBConnection::MongoDBConnection(MongoDBConnection &&other) noexcept
            : m_client(std::move(other.m_client)),
              m_databaseName(std::move(other.m_databaseName)),
              m_url(std::move(other.m_url)),
              m_closed(other.m_closed.load()),
              m_pooled(other.m_pooled),
              m_sessions(std::move(other.m_sessions)),
              m_sessionCounter(other.m_sessionCounter.load())
        {
            other.m_closed = true;
        }

        MongoDBConnection &MongoDBConnection::operator=(MongoDBConnection &&other) noexcept
        {
            if (this != &other)
            {
                if (!m_closed)
                    close();

                m_client = std::move(other.m_client);
                m_databaseName = std::move(other.m_databaseName);
                m_url = std::move(other.m_url);
                m_closed = other.m_closed.load();
                m_pooled = other.m_pooled;
                m_sessions = std::move(other.m_sessions);
                m_sessionCounter = other.m_sessionCounter.load();

                other.m_closed = true;
            }
            return *this;
        }

        void MongoDBConnection::validateConnection() const
        {
            if (m_closed)
            {
                throw DBException("M7N8O9P0Q1R2", "MongoDB connection is closed", system_utils::captureCallStack());
            }
        }

        std::string MongoDBConnection::generateSessionId()
        {
            uint64_t id = m_sessionCounter.fetch_add(1);
            std::ostringstream oss;
            oss << "session_" << id << "_" << std::chrono::steady_clock::now().time_since_epoch().count();
            return oss.str();
        }

        void MongoDBConnection::close()
        {
            MONGODB_LOCK_GUARD(m_connMutex);

            if (m_closed)
                return;

            // End all active sessions
            {
                std::lock_guard<std::mutex> sessionsLock(m_sessionsMutex);
                m_sessions.clear();
            }

            // Clear active collections
            {
                std::lock_guard<std::mutex> collectionsLock(m_collectionsMutex);
                m_activeCollections.clear();
            }

            m_client.reset();
            m_closed = true;
        }

        bool MongoDBConnection::isClosed()
        {
            return m_closed;
        }

        void MongoDBConnection::returnToPool()
        {
            // If pooled, just mark as available (pool handles this)
            // If not pooled, close the connection
            if (!m_pooled)
            {
                close();
            }
        }

        bool MongoDBConnection::isPooled()
        {
            return m_pooled;
        }

        std::string MongoDBConnection::getURL() const
        {
            return m_url;
        }

        std::string MongoDBConnection::getDatabaseName() const
        {
            MONGODB_LOCK_GUARD(m_connMutex);
            return m_databaseName;
        }

        std::vector<std::string> MongoDBConnection::listDatabases()
        {
            MONGODB_LOCK_GUARD(m_connMutex);
            validateConnection();

            std::vector<std::string> result;

            bson_error_t error;
            char **names = mongoc_client_get_database_names_with_opts(m_client.get(), nullptr, &error);

            if (!names)
            {
                throw DBException("N8O9P0Q1R2S3", std::string("Failed to list databases: ") + error.message, system_utils::captureCallStack());
            }

            for (char **ptr = names; *ptr; ptr++)
            {
                result.emplace_back(*ptr);
            }

            bson_strfreev(names);
            return result;
        }

        bool MongoDBConnection::databaseExists(const std::string &databaseName)
        {
            auto databases = listDatabases();
            return std::find(databases.begin(), databases.end(), databaseName) != databases.end();
        }

        void MongoDBConnection::useDatabase(const std::string &databaseName)
        {
            MONGODB_LOCK_GUARD(m_connMutex);
            validateConnection();
            m_databaseName = databaseName;
        }

        void MongoDBConnection::dropDatabase(const std::string &databaseName)
        {
            MONGODB_LOCK_GUARD(m_connMutex);
            validateConnection();

            MongoDatabaseHandle db(mongoc_client_get_database(m_client.get(), databaseName.c_str()));

            bson_error_t error;
            bool success = mongoc_database_drop(db.get(), &error);

            if (!success)
            {
                throw DBException("O9P0Q1R2S3T4", std::string("Failed to drop database: ") + error.message, system_utils::captureCallStack());
            }
        }

        std::shared_ptr<DocumentDBCollection> MongoDBConnection::getCollection(const std::string &collectionName)
        {
            MONGODB_LOCK_GUARD(m_connMutex);
            validateConnection();

            if (m_databaseName.empty())
            {
                throw DBException("P0Q1R2S3T4U5", "No database selected - call useDatabase() first", system_utils::captureCallStack());
            }

            mongoc_collection_t *coll = mongoc_client_get_collection(
                m_client.get(), m_databaseName.c_str(), collectionName.c_str());

            if (!coll)
            {
                throw DBException("Q1R2S3T4U5V6", "Failed to get collection: " + collectionName, system_utils::captureCallStack());
            }

            return std::make_shared<MongoDBCollection>(
                std::weak_ptr<mongoc_client_t>(m_client), coll, collectionName, m_databaseName);
        }

        std::vector<std::string> MongoDBConnection::listCollections()
        {
            MONGODB_LOCK_GUARD(m_connMutex);
            validateConnection();

            if (m_databaseName.empty())
            {
                throw DBException("R2S3T4U5V6W7", "No database selected - call useDatabase() first", system_utils::captureCallStack());
            }

            std::vector<std::string> result;

            MongoDatabaseHandle db(mongoc_client_get_database(m_client.get(), m_databaseName.c_str()));

            bson_error_t error;
            char **names = mongoc_database_get_collection_names_with_opts(db.get(), nullptr, &error);

            if (!names)
            {
                throw DBException("S3T4U5V6W7X8", std::string("Failed to list collections: ") + error.message, system_utils::captureCallStack());
            }

            for (char **ptr = names; *ptr; ptr++)
            {
                result.emplace_back(*ptr);
            }

            bson_strfreev(names);
            return result;
        }

        bool MongoDBConnection::collectionExists(const std::string &collectionName)
        {
            auto collections = listCollections();
            return std::find(collections.begin(), collections.end(), collectionName) != collections.end();
        }

        std::shared_ptr<DocumentDBCollection> MongoDBConnection::createCollection(
            const std::string &collectionName,
            const std::string &options)
        {
            MONGODB_LOCK_GUARD(m_connMutex);
            validateConnection();

            if (m_databaseName.empty())
            {
                throw DBException("T4U5V6W7X8Y9", "No database selected - call useDatabase() first", system_utils::captureCallStack());
            }

            MongoDatabaseHandle db(mongoc_client_get_database(m_client.get(), m_databaseName.c_str()));

            bson_t *opts = nullptr;
            if (!options.empty())
            {
                bson_error_t parseError;
                opts = bson_new_from_json(
                    reinterpret_cast<const uint8_t *>(options.c_str()),
                    static_cast<ssize_t>(options.length()),
                    &parseError);
                if (!opts)
                {
                    throw DBException("U5V6W7X8Y9Z0", std::string("Invalid options JSON: ") + parseError.message, system_utils::captureCallStack());
                }
            }

            bson_error_t error;
            mongoc_collection_t *coll = mongoc_database_create_collection(
                db.get(), collectionName.c_str(), opts, &error);

            if (opts)
                bson_destroy(opts);

            if (!coll)
            {
                throw DBException("V6W7X8Y9Z0A1", std::string("Failed to create collection: ") + error.message, system_utils::captureCallStack());
            }

            return std::make_shared<MongoDBCollection>(
                std::weak_ptr<mongoc_client_t>(m_client), coll, collectionName, m_databaseName);
        }

        void MongoDBConnection::dropCollection(const std::string &collectionName)
        {
            MONGODB_LOCK_GUARD(m_connMutex);
            validateConnection();

            if (m_databaseName.empty())
            {
                throw DBException("W7X8Y9Z0A1B2", "No database selected - call useDatabase() first", system_utils::captureCallStack());
            }

            mongoc_collection_t *coll = mongoc_client_get_collection(
                m_client.get(), m_databaseName.c_str(), collectionName.c_str());

            if (!coll)
            {
                throw DBException("X8Y9Z0A1B2C3", "Failed to get collection: " + collectionName, system_utils::captureCallStack());
            }

            MongoCollectionHandle collHandle(coll);

            bson_error_t error;
            bool success = mongoc_collection_drop(collHandle.get(), &error);

            if (!success)
            {
                throw DBException("Y9Z0A1B2C3D4", std::string("Failed to drop collection: ") + error.message, system_utils::captureCallStack());
            }
        }

        std::shared_ptr<DocumentDBData> MongoDBConnection::createDocument()
        {
            return std::make_shared<MongoDBDocument>();
        }

        std::shared_ptr<DocumentDBData> MongoDBConnection::createDocument(const std::string &json)
        {
            return std::make_shared<MongoDBDocument>(json);
        }

        std::shared_ptr<DocumentDBData> MongoDBConnection::runCommand(const std::string &command)
        {
            MONGODB_LOCK_GUARD(m_connMutex);
            validateConnection();

            if (m_databaseName.empty())
            {
                throw DBException("Z0A1B2C3D4E5", "No database selected - call useDatabase() first", system_utils::captureCallStack());
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
                throw DBException("A1B2C3D4E5F6", std::string("Command failed: ") + error.message, system_utils::captureCallStack());
            }

            bson_t *replyCopy = bson_copy(&reply);
            bson_destroy(&reply);

            return std::make_shared<MongoDBDocument>(replyCopy);
        }

        std::shared_ptr<DocumentDBData> MongoDBConnection::getServerInfo()
        {
            return runCommand("{\"buildInfo\": 1}");
        }

        std::shared_ptr<DocumentDBData> MongoDBConnection::getServerStatus()
        {
            return runCommand("{\"serverStatus\": 1}");
        }

        bool MongoDBConnection::ping()
        {
            MONGODB_LOCK_GUARD(m_connMutex);

            if (m_closed)
                return false;

            bson_t pingCmd = BSON_INITIALIZER;
            BSON_APPEND_INT32(&pingCmd, "ping", 1);

            bson_t reply;
            bson_init(&reply);

            MongoDatabaseHandle adminDb(mongoc_client_get_database(m_client.get(), "admin"));

            bson_error_t error;
            bool success = mongoc_database_command_simple(adminDb.get(), &pingCmd, nullptr, &reply, &error);

            bson_destroy(&pingCmd);
            bson_destroy(&reply);

            return success;
        }

        std::string MongoDBConnection::startSession()
        {
            MONGODB_LOCK_GUARD(m_connMutex);
            validateConnection();

            mongoc_session_opt_t *opts = mongoc_session_opts_new();
            mongoc_session_opts_set_causal_consistency(opts, true);

            bson_error_t error;
            mongoc_client_session_t *session = mongoc_client_start_session(m_client.get(), opts, &error);

            mongoc_session_opts_destroy(opts);

            if (!session)
            {
                throw DBException("B2C3D4E5F6G7", std::string("Failed to start session: ") + error.message, system_utils::captureCallStack());
            }

            std::string sessionId = generateSessionId();

            {
                std::lock_guard<std::mutex> sessionsLock(m_sessionsMutex);
                m_sessions[sessionId] = MongoSessionHandle(session);
            }

            return sessionId;
        }

        void MongoDBConnection::endSession(const std::string &sessionId)
        {
            std::lock_guard<std::mutex> lock(m_sessionsMutex);

            auto it = m_sessions.find(sessionId);
            if (it != m_sessions.end())
            {
                m_sessions.erase(it);
            }
        }

        void MongoDBConnection::startTransaction(const std::string &sessionId)
        {
            std::lock_guard<std::mutex> lock(m_sessionsMutex);

            auto it = m_sessions.find(sessionId);
            if (it == m_sessions.end())
            {
                throw DBException("C3D4E5F6G7H8", "Session not found: " + sessionId, system_utils::captureCallStack());
            }

            bson_error_t error;
            bool success = mongoc_client_session_start_transaction(it->second.get(), nullptr, &error);

            if (!success)
            {
                throw DBException("D4E5F6G7H8I9", std::string("Failed to start transaction: ") + error.message, system_utils::captureCallStack());
            }
        }

        void MongoDBConnection::commitTransaction(const std::string &sessionId)
        {
            std::lock_guard<std::mutex> lock(m_sessionsMutex);

            auto it = m_sessions.find(sessionId);
            if (it == m_sessions.end())
            {
                throw DBException("E5F6G7H8I9J0", "Session not found: " + sessionId, system_utils::captureCallStack());
            }

            bson_t reply;
            bson_init(&reply);

            bson_error_t error;
            bool success = mongoc_client_session_commit_transaction(it->second.get(), &reply, &error);

            bson_destroy(&reply);

            if (!success)
            {
                throw DBException("F6G7H8I9J0K1", std::string("Failed to commit transaction: ") + error.message, system_utils::captureCallStack());
            }
        }

        void MongoDBConnection::abortTransaction(const std::string &sessionId)
        {
            std::lock_guard<std::mutex> lock(m_sessionsMutex);

            auto it = m_sessions.find(sessionId);
            if (it == m_sessions.end())
            {
                throw DBException("G7H8I9J0K1L2", "Session not found: " + sessionId, system_utils::captureCallStack());
            }

            bson_error_t error;
            bool success = mongoc_client_session_abort_transaction(it->second.get(), &error);

            if (!success)
            {
                throw DBException("H8I9J0K1L2M3", std::string("Failed to abort transaction: ") + error.message, system_utils::captureCallStack());
            }
        }

        bool MongoDBConnection::supportsTransactions()
        {
            // MongoDB 4.0+ supports multi-document transactions
            // For simplicity, return true (actual check would require server version)
            return true;
        }

        std::weak_ptr<mongoc_client_t> MongoDBConnection::getClientWeak() const
        {
            return std::weak_ptr<mongoc_client_t>(m_client);
        }

        MongoClientHandle MongoDBConnection::getClient() const
        {
            return m_client;
        }

        void MongoDBConnection::setPooled(bool pooled)
        {
            m_pooled = pooled;
        }

        // ============================================================================
        // MongoDBDriver Implementation
        // ============================================================================

        void MongoDBDriver::initializeMongoc()
        {
            MONGODB_DEBUG("MongoDBDriver::initializeMongoc - Initializing MongoDB C driver");
            mongoc_init();
            s_initialized = true;
            MONGODB_DEBUG("MongoDBDriver::initializeMongoc - Done");
        }

        MongoDBDriver::MongoDBDriver()
        {
            MONGODB_DEBUG("MongoDBDriver::constructor - Creating driver");
            std::call_once(s_initFlag, initializeMongoc);
            MONGODB_DEBUG("MongoDBDriver::constructor - Done");
        }

        MongoDBDriver::~MongoDBDriver()
        {
            MONGODB_DEBUG("MongoDBDriver::destructor - Destroying driver");
        }

        std::shared_ptr<DBConnection> MongoDBDriver::connect(
            const std::string &url,
            const std::string &user,
            const std::string &password,
            const std::map<std::string, std::string> &options)
        {
            return connectDocument(url, user, password, options);
        }

        bool MongoDBDriver::acceptsURL(const std::string &url)
        {
            return url.find("mongodb://") == 0 || url.find("mongodb+srv://") == 0;
        }

        std::shared_ptr<DocumentDBConnection> MongoDBDriver::connectDocument(
            const std::string &url,
            const std::string &user,
            const std::string &password,
            const std::map<std::string, std::string> &options)
        {
            MONGODB_DEBUG("MongoDBDriver::connectDocument - Connecting to: " << url);
            MONGODB_LOCK_GUARD(m_mutex);

            if (!acceptsURL(url))
            {
                throw DBException("I9J0K1L2M3N4", "Invalid MongoDB URL: " + url, system_utils::captureCallStack());
            }

            auto conn = std::make_shared<MongoDBConnection>(url, user, password, options);
            MONGODB_DEBUG("MongoDBDriver::connectDocument - Connection established");
            return conn;
        }

        int MongoDBDriver::getDefaultPort() const
        {
            return 27017;
        }

        std::string MongoDBDriver::getURIScheme() const
        {
            return "mongodb";
        }

        std::map<std::string, std::string> MongoDBDriver::parseURI(const std::string &uri)
        {
            std::map<std::string, std::string> result;

            bson_error_t error;
            MongoUriHandle mongoUri(mongoc_uri_new_with_error(uri.c_str(), &error));

            if (!mongoUri)
            {
                throw DBException("J0K1L2M3N4O5", std::string("Invalid URI: ") + error.message, system_utils::captureCallStack());
            }

            // Extract components
            const char *host = mongoc_uri_get_hosts(mongoUri.get())->host;
            if (host)
                result["host"] = host;

            uint16_t port = mongoc_uri_get_hosts(mongoUri.get())->port;
            result["port"] = std::to_string(port);

            const char *database = mongoc_uri_get_database(mongoUri.get());
            if (database)
                result["database"] = database;

            const char *username = mongoc_uri_get_username(mongoUri.get());
            if (username)
                result["username"] = username;

            const char *authSource = mongoc_uri_get_auth_source(mongoUri.get());
            if (authSource)
                result["authSource"] = authSource;

            const char *replicaSet = mongoc_uri_get_replica_set(mongoUri.get());
            if (replicaSet)
                result["replicaSet"] = replicaSet;

            return result;
        }

        std::string MongoDBDriver::buildURI(
            const std::string &host,
            int port,
            const std::string &database,
            const std::map<std::string, std::string> &options)
        {
            std::ostringstream uri;

            // Start with scheme
            uri << "mongodb://";

            // Add host
            uri << (host.empty() ? "localhost" : host);

            // Add port
            if (port > 0)
            {
                uri << ":" << port;
            }

            // Add database
            if (!database.empty())
            {
                uri << "/" << database;
            }

            // Add options
            bool firstOption = true;
            for (const auto &opt : options)
            {
                uri << (firstOption ? "?" : "&");
                uri << opt.first << "=" << opt.second;
                firstOption = false;
            }

            return uri.str();
        }

        bool MongoDBDriver::supportsReplicaSets() const
        {
            return true;
        }

        bool MongoDBDriver::supportsSharding() const
        {
            return true;
        }

        std::string MongoDBDriver::getDriverVersion() const
        {
            return MONGOC_VERSION_S;
        }

        void MongoDBDriver::cleanup()
        {
            MONGODB_DEBUG("MongoDBDriver::cleanup - Cleaning up MongoDB C driver");
            if (s_initialized)
            {
                mongoc_cleanup();
                s_initialized = false;
                MONGODB_DEBUG("MongoDBDriver::cleanup - Done");
            }
        }

        bool MongoDBDriver::isInitialized()
        {
            return s_initialized;
        }

        bool MongoDBDriver::validateURI(const std::string &uri)
        {
            bson_error_t error;
            mongoc_uri_t *mongoUri = mongoc_uri_new_with_error(uri.c_str(), &error);

            if (mongoUri)
            {
                mongoc_uri_destroy(mongoUri);
                return true;
            }

            return false;
        }

    } // namespace MongoDB
} // namespace cpp_dbc

#endif // USE_MONGODB