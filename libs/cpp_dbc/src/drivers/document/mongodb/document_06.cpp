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
 * @file document_06.cpp
 * @brief MongoDB MongoDBDocument - Part 6 (nothrow getters part 2, clone)
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

    expected<std::vector<std::shared_ptr<DocumentDBData>>, DBException> MongoDBDocument::getDocumentArray(
        std::nothrow_t, const std::string &fieldPath, bool strict) const noexcept
    {
        try
        {
            MONGODB_LOCK_GUARD(m_mutex);

            bson_iter_t iter;
            if (!navigateToField(fieldPath, iter))
            {
                return unexpected<DBException>(DBException(
                    "4D5E6F7A8B9C",
                    "Field not found: " + fieldPath));
            }

            if (!BSON_ITER_HOLDS_ARRAY(&iter))
            {
                return unexpected<DBException>(DBException(
                    "5E6F7A8B9C0D",
                    "Field is not an array: " + fieldPath));
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
                    "6F7A8B9C0D1E",
                    "Failed to initialize array BSON"));
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
                                "Failed to construct subdocument at index " + std::to_string(elementIndex) + " in array field: " + fieldPath));
                        }
                        result.push_back(std::make_shared<MongoDBDocument>(subdoc));
                    }
                    else if (strict)
                    {
                        return unexpected<DBException>(DBException(
                            "7A8B9C0D1E2F",
                            "Unexpected element type at index " + std::to_string(elementIndex) + " in array field: " + fieldPath + " (expected document)"));
                    }
                    // If not strict, skip non-document elements
                    elementIndex++;
                }
            }

            return result;
        }
        catch (const DBException &ex)
        {
            return unexpected<DBException>(ex);
        }
        catch ([[maybe_unused]] const std::bad_alloc &ex)
        {
            return unexpected<DBException>(DBException(
                "M9N0O1P2Q3R4",
                "Memory allocation failed in getDocumentArray"));
        }
        catch (const std::exception &ex)
        {
            return unexpected<DBException>(DBException(
                "8B9C0D1E2F3A",
                std::string("Error in getDocumentArray: ") + ex.what()));
        }
        catch (...)
        {
            return unexpected<DBException>(DBException(
                "9C0D1E2F3A4B",
                "Unknown error in getDocumentArray"));
        }
    }

    expected<std::vector<std::shared_ptr<DocumentDBData>>, DBException> MongoDBDocument::getDocumentArray(
        std::nothrow_t, const std::string &fieldPath) const noexcept
    {
        // Default: tolerant mode (skip non-document elements)
        return getDocumentArray(std::nothrow, fieldPath, false);
    }

    expected<std::vector<std::string>, DBException> MongoDBDocument::getStringArray(
        std::nothrow_t, const std::string &fieldPath, bool strict) const noexcept
    {
        try
        {
            MONGODB_LOCK_GUARD(m_mutex);

            bson_iter_t iter;
            if (!navigateToField(fieldPath, iter))
            {
                return unexpected<DBException>(DBException(
                    "0D1E2F3A4B5C",
                    "Field not found: " + fieldPath));
            }

            if (!BSON_ITER_HOLDS_ARRAY(&iter))
            {
                return unexpected<DBException>(DBException(
                    "1E2F3A4B5C6D",
                    "Field is not an array: " + fieldPath));
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
                    "2F3A4B5C6D7E",
                    "Failed to initialize array BSON"));
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
                            "Unexpected element type at index " + std::to_string(elementIndex) + " in array field: " + fieldPath + " (expected string)"));
                    }
                    // If not strict, skip non-string elements
                    elementIndex++;
                }
            }

            return result;
        }
        catch (const DBException &ex)
        {
            return unexpected<DBException>(ex);
        }
        catch ([[maybe_unused]] const std::bad_alloc &ex)
        {
            return unexpected<DBException>(DBException(
                "3A4B5C6D7E8F",
                "Memory allocation failed in getStringArray"));
        }
        catch (const std::exception &ex)
        {
            return unexpected<DBException>(DBException(
                "4B5C6D7E8F9A",
                std::string("Error in getStringArray: ") + ex.what()));
        }
        catch (...)
        {
            return unexpected<DBException>(DBException(
                "5C6D7E8F9A0B",
                "Unknown error in getStringArray"));
        }
    }

    expected<std::vector<std::string>, DBException> MongoDBDocument::getStringArray(
        std::nothrow_t, const std::string &fieldPath) const noexcept
    {
        // Default: tolerant mode (skip non-string elements)
        return getStringArray(std::nothrow, fieldPath, false);
    }

    expected<std::shared_ptr<DocumentDBData>, DBException> MongoDBDocument::clone(std::nothrow_t) const noexcept
    {
        try
        {
            MONGODB_LOCK_GUARD(m_mutex);

            if (!m_bson)
            {
                return unexpected<DBException>(DBException(
                    "6D7E8F9A0B1C",
                    "Document is not initialized"));
            }

            bson_t *copy = bson_copy(m_bson.get());
            if (!copy)
            {
                return unexpected<DBException>(DBException(
                    "7E8F9A0B1C2D",
                    "Failed to clone document"));
            }

            auto doc = std::make_shared<MongoDBDocument>(copy);
            return std::static_pointer_cast<DocumentDBData>(doc);
        }
        catch (const DBException &ex)
        {
            return unexpected<DBException>(ex);
        }
        catch ([[maybe_unused]] const std::bad_alloc &ex)
        {
            return unexpected<DBException>(DBException(
                "8F9A0B1C2D3E",
                "Memory allocation failed in clone"));
        }
        catch (const std::exception &ex)
        {
            return unexpected<DBException>(DBException(
                "9A0B1C2D3E4F",
                std::string("Error in clone: ") + ex.what()));
        }
        catch (...)
        {
            return unexpected<DBException>(DBException(
                "0B1C2D3E4F5A",
                "Unknown error in clone"));
        }
    }

} // namespace cpp_dbc::MongoDB

#endif // USE_MONGODB
