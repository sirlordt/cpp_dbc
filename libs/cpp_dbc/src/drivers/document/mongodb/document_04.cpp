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
 * @file document_04.cpp
 * @brief MongoDB MongoDBDocument - Part 4 (field operations, utilities - throwing wrappers)
 */

#include "cpp_dbc/drivers/document/driver_mongodb.hpp"

#if USE_MONGODB

#include "cpp_dbc/common/system_utils.hpp"
#include "mongodb_internal.hpp"

namespace cpp_dbc::MongoDB
{

    #ifdef __cpp_exceptions
    bool MongoDBDocument::hasField(const std::string &fieldPath) const
    {
        auto r = hasField(std::nothrow, fieldPath);
        if (!r.has_value())
        {
            throw r.error();
        }
        return *r;
    }

    bool MongoDBDocument::isNull(const std::string &fieldPath) const
    {
        auto r = isNull(std::nothrow, fieldPath);
        if (!r.has_value())
        {
            throw r.error();
        }
        return *r;
    }

    bool MongoDBDocument::removeField(const std::string &fieldPath)
    {
        auto r = removeField(std::nothrow, fieldPath);
        if (!r.has_value())
        {
            throw r.error();
        }
        return *r;
    }

    std::vector<std::string> MongoDBDocument::getFieldNames() const
    {
        auto r = getFieldNames(std::nothrow);
        if (!r.has_value())
        {
            throw r.error();
        }
        return *r;
    }

    std::shared_ptr<DocumentDBData> MongoDBDocument::clone() const
    {
        auto r = clone(std::nothrow);
        if (!r.has_value())
        {
            throw r.error();
        }
        return *r;
    }

    void MongoDBDocument::clear()
    {
        auto r = clear(std::nothrow);
        if (!r.has_value())
        {
            throw r.error();
        }
    }

    bool MongoDBDocument::isEmpty() const
    {
        auto r = isEmpty(std::nothrow);
        if (!r.has_value())
        {
            throw r.error();
        }
        return *r;
    }
    #endif // __cpp_exceptions

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

    expected<std::shared_ptr<MongoDBDocument>, DBException>
    MongoDBDocument::fromBson(std::nothrow_t, bson_t *bson) noexcept
    {
        if (!bson)
        {
            return unexpected<DBException>(DBException(
                "TIPPT5AZJ1I1",
                "Cannot create document from null BSON pointer",
                system_utils::captureCallStack()));
        }
        return std::make_shared<MongoDBDocument>(bson);
    }

    expected<std::shared_ptr<MongoDBDocument>, DBException>
    MongoDBDocument::fromBsonCopy(std::nothrow_t, const bson_t *bson) noexcept
    {
        if (!bson)
        {
            return unexpected<DBException>(DBException(
                "Z99M25OOHIBD",
                "Cannot create document from null BSON pointer",
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

        return std::make_shared<MongoDBDocument>(copy);
    }

} // namespace cpp_dbc::MongoDB

#endif // USE_MONGODB
