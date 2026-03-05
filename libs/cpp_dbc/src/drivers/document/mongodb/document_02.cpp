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
 * @brief MongoDB MongoDBDocument - Part 2 (getters throwing wrappers:
 *        getString, getInt, getDouble, getBool, getBinary,
 *        getDocument, getDocumentArray, getStringArray)
 */

#include "cpp_dbc/drivers/document/driver_mongodb.hpp"

#if USE_MONGODB

#include "cpp_dbc/common/system_utils.hpp"
#include "mongodb_internal.hpp"

namespace cpp_dbc::MongoDB
{

#ifdef __cpp_exceptions

    std::string MongoDBDocument::getString(const std::string &fieldPath) const
    {
        auto r = getString(std::nothrow, fieldPath);
        if (!r.has_value())
        {
            throw r.error();
        }
        return *r;
    }

    int64_t MongoDBDocument::getInt(const std::string &fieldPath) const
    {
        auto r = getInt(std::nothrow, fieldPath);
        if (!r.has_value())
        {
            throw r.error();
        }
        return *r;
    }

    double MongoDBDocument::getDouble(const std::string &fieldPath) const
    {
        auto r = getDouble(std::nothrow, fieldPath);
        if (!r.has_value())
        {
            throw r.error();
        }
        return *r;
    }

    bool MongoDBDocument::getBool(const std::string &fieldPath) const
    {
        auto r = getBool(std::nothrow, fieldPath);
        if (!r.has_value())
        {
            throw r.error();
        }
        return *r;
    }

    std::vector<uint8_t> MongoDBDocument::getBinary(const std::string &fieldPath) const
    {
        auto r = getBinary(std::nothrow, fieldPath);
        if (!r.has_value())
        {
            throw r.error();
        }
        return *r;
    }

    std::shared_ptr<DocumentDBData> MongoDBDocument::getDocument(const std::string &fieldPath) const
    {
        auto r = getDocument(std::nothrow, fieldPath);
        if (!r.has_value())
        {
            throw r.error();
        }
        return *r;
    }

    std::vector<std::shared_ptr<DocumentDBData>> MongoDBDocument::getDocumentArray(const std::string &fieldPath) const
    {
        auto r = getDocumentArray(std::nothrow, fieldPath);
        if (!r.has_value())
        {
            throw r.error();
        }
        return *r;
    }

    std::vector<std::string> MongoDBDocument::getStringArray(const std::string &fieldPath) const
    {
        auto r = getStringArray(std::nothrow, fieldPath);
        if (!r.has_value())
        {
            throw r.error();
        }
        return *r;
    }
#endif // __cpp_exceptions

} // namespace cpp_dbc::MongoDB

#endif // USE_MONGODB
