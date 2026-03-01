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
 * @brief MongoDB MongoDBDocument - Part 4 (hasField, isNull, removeField, getFieldNames,
 *        clone, clear, isEmpty - throwing wrappers)
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

} // namespace cpp_dbc::MongoDB

#endif // USE_MONGODB
