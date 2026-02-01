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
 * @file document_02.cpp
 * @brief MongoDB MongoDBDocument - Part 2 (getters)
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

} // namespace cpp_dbc::MongoDB

#endif // USE_MONGODB
