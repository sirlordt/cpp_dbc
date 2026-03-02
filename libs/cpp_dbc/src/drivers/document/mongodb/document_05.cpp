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
 * @brief MongoDB MongoDBDocument - Part 5 (nothrow getters part 1)
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

    // ====================================================================
    // MongoDBDocument NOTHROW VERSIONS
    // ====================================================================

    expected<std::string, DBException> MongoDBDocument::getString(std::nothrow_t, const std::string &fieldPath) const noexcept
    {
        try
        {
            MONGODB_LOCK_GUARD(m_mutex);

            bson_iter_t iter;
            if (!navigateToField(fieldPath, iter))
            {
                return unexpected<DBException>(DBException(
                    "P2Q3R4S5T6U7",
                    "Field not found: " + fieldPath));
            }

            if (!BSON_ITER_HOLDS_UTF8(&iter))
            {
                return unexpected<DBException>(DBException(
                    "8B9C0D1E2F3A",
                    "Field is not a string: " + fieldPath));
            }

            uint32_t length = 0;
            const char *str = bson_iter_utf8(&iter, &length);
            return std::string(str, length);
        }
        catch (const DBException &ex)
        {
            return unexpected<DBException>(ex);
        }
        catch (const std::exception &ex)
        {
            return unexpected<DBException>(DBException(
                "9C0D1E2F3A4B",
                std::string("Error in getString: ") + ex.what()));
        }
        catch (...)
        {
            return unexpected<DBException>(DBException(
                "0D1E2F3A4B5C",
                "Unknown error in getString"));
        }
    }

    expected<int64_t, DBException> MongoDBDocument::getInt(std::nothrow_t, const std::string &fieldPath) const noexcept
    {
        try
        {
            MONGODB_LOCK_GUARD(m_mutex);

            bson_iter_t iter;
            if (!navigateToField(fieldPath, iter))
            {
                return unexpected<DBException>(DBException(
                    "1E2F3A4B5C6D",
                    "Field not found: " + fieldPath));
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
                return unexpected<DBException>(DBException(
                    "2F3A4B5C6D7E",
                    "Field is not an integer: " + fieldPath));
            }
        }
        catch (const DBException &ex)
        {
            return unexpected<DBException>(ex);
        }
        catch (const std::exception &ex)
        {
            return unexpected<DBException>(DBException(
                "3A4B5C6D7E8F",
                std::string("Error in getInt: ") + ex.what()));
        }
        catch (...)
        {
            return unexpected<DBException>(DBException(
                "4B5C6D7E8F9A",
                "Unknown error in getInt"));
        }
    }

    expected<double, DBException> MongoDBDocument::getDouble(std::nothrow_t, const std::string &fieldPath) const noexcept
    {
        try
        {
            MONGODB_LOCK_GUARD(m_mutex);

            bson_iter_t iter;
            if (!navigateToField(fieldPath, iter))
            {
                return unexpected<DBException>(DBException(
                    "5C6D7E8F9A0B",
                    "Field not found: " + fieldPath));
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
                return unexpected<DBException>(DBException(
                    "6D7E8F9A0B1C",
                    "Field is not a number: " + fieldPath));
            }
        }
        catch (const DBException &ex)
        {
            return unexpected<DBException>(ex);
        }
        catch (const std::exception &ex)
        {
            return unexpected<DBException>(DBException(
                "7E8F9A0B1C2D",
                std::string("Error in getDouble: ") + ex.what()));
        }
        catch (...)
        {
            return unexpected<DBException>(DBException(
                "8F9A0B1C2D3E",
                "Unknown error in getDouble"));
        }
    }

    expected<bool, DBException> MongoDBDocument::getBool(std::nothrow_t, const std::string &fieldPath) const noexcept
    {
        try
        {
            MONGODB_LOCK_GUARD(m_mutex);

            bson_iter_t iter;
            if (!navigateToField(fieldPath, iter))
            {
                return unexpected<DBException>(DBException(
                    "9A0B1C2D3E4F",
                    "Field not found: " + fieldPath));
            }

            if (!BSON_ITER_HOLDS_BOOL(&iter))
            {
                return unexpected<DBException>(DBException(
                    "0B1C2D3E4F5A",
                    "Field is not a boolean: " + fieldPath));
            }

            return bson_iter_bool(&iter);
        }
        catch (const DBException &ex)
        {
            return unexpected<DBException>(ex);
        }
        catch (const std::exception &ex)
        {
            return unexpected<DBException>(DBException(
                "1C2D3E4F5A6B",
                std::string("Error in getBool: ") + ex.what()));
        }
        catch (...)
        {
            return unexpected<DBException>(DBException(
                "2D3E4F5A6B7C",
                "Unknown error in getBool"));
        }
    }

    expected<std::vector<uint8_t>, DBException> MongoDBDocument::getBinary(std::nothrow_t, const std::string &fieldPath) const noexcept
    {
        try
        {
            MONGODB_LOCK_GUARD(m_mutex);

            bson_iter_t iter;
            if (!navigateToField(fieldPath, iter))
            {
                return unexpected<DBException>(DBException(
                    "3E4F5A6B7C8D",
                    "Field not found: " + fieldPath));
            }

            if (!BSON_ITER_HOLDS_BINARY(&iter))
            {
                return unexpected<DBException>(DBException(
                    "4F5A6B7C8D9E",
                    "Field is not binary: " + fieldPath));
            }

            bson_subtype_t subtype;
            uint32_t length = 0;
            const uint8_t *data = nullptr;
            bson_iter_binary(&iter, &subtype, &length, &data);

            return std::vector<uint8_t>(data, data + length);
        }
        catch (const DBException &ex)
        {
            return unexpected<DBException>(ex);
        }
        catch ([[maybe_unused]] const std::bad_alloc &ex)
        {
            return unexpected<DBException>(DBException(
                "5A6B7C8D9E0F",
                "Memory allocation failed in getBinary"));
        }
        catch (const std::exception &ex)
        {
            return unexpected<DBException>(DBException(
                "6B7C8D9E0F1A",
                std::string("Error in getBinary: ") + ex.what()));
        }
        catch (...)
        {
            return unexpected<DBException>(DBException(
                "7C8D9E0F1A2B",
                "Unknown error in getBinary"));
        }
    }

    expected<std::shared_ptr<DocumentDBData>, DBException> MongoDBDocument::getDocument(std::nothrow_t, const std::string &fieldPath) const noexcept
    {
        try
        {
            MONGODB_LOCK_GUARD(m_mutex);

            bson_iter_t iter;
            if (!navigateToField(fieldPath, iter))
            {
                return unexpected<DBException>(DBException(
                    "8D9E0F1A2B3C",
                    "Field not found: " + fieldPath));
            }

            if (!BSON_ITER_HOLDS_DOCUMENT(&iter))
            {
                return unexpected<DBException>(DBException(
                    "9E0F1A2B3C4D",
                    "Field is not a document: " + fieldPath));
            }

            const uint8_t *data = nullptr;
            uint32_t length = 0;
            bson_iter_document(&iter, &length, &data);

            bson_t *subdoc = bson_new_from_data(data, length);
            if (!subdoc)
            {
                return unexpected<DBException>(DBException(
                    "0F1A2B3C4D5E",
                    "Failed to extract subdocument"));
            }

            auto doc = std::make_shared<MongoDBDocument>(subdoc);
            return std::static_pointer_cast<DocumentDBData>(doc);
        }
        catch (const DBException &ex)
        {
            return unexpected<DBException>(ex);
        }
        catch ([[maybe_unused]] const std::bad_alloc &ex)
        {
            return unexpected<DBException>(DBException(
                "1A2B3C4D5E6F",
                "Memory allocation failed in getDocument"));
        }
        catch (const std::exception &ex)
        {
            return unexpected<DBException>(DBException(
                "2B3C4D5E6F7A",
                std::string("Error in getDocument: ") + ex.what()));
        }
        catch (...)
        {
            return unexpected<DBException>(DBException(
                "3C4D5E6F7A8B",
                "Unknown error in getDocument"));
        }
    }

} // namespace cpp_dbc::MongoDB

#endif // USE_MONGODB
