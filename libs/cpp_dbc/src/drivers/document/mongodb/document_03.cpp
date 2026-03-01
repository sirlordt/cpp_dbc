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
 * @file document_03.cpp
 * @brief MongoDB MongoDBDocument - Part 3 (setters throwing wrappers:
 *        setString, setInt, setDouble, setBool, setBinary, setDocument, setNull)
 */

#include "cpp_dbc/drivers/document/driver_mongodb.hpp"

#if USE_MONGODB

#include "cpp_dbc/common/system_utils.hpp"
#include "mongodb_internal.hpp"

namespace cpp_dbc::MongoDB
{

#ifdef __cpp_exceptions

    void MongoDBDocument::setString(const std::string &fieldPath, const std::string &value)
    {
        auto r = setString(std::nothrow, fieldPath, value);
        if (!r.has_value())
        {
            throw r.error();
        }
    }

    void MongoDBDocument::setInt(const std::string &fieldPath, int64_t value)
    {
        auto r = setInt(std::nothrow, fieldPath, value);
        if (!r.has_value())
        {
            throw r.error();
        }
    }

    void MongoDBDocument::setDouble(const std::string &fieldPath, double value)
    {
        auto r = setDouble(std::nothrow, fieldPath, value);
        if (!r.has_value())
        {
            throw r.error();
        }
    }

    void MongoDBDocument::setBool(const std::string &fieldPath, bool value)
    {
        auto r = setBool(std::nothrow, fieldPath, value);
        if (!r.has_value())
        {
            throw r.error();
        }
    }

    void MongoDBDocument::setBinary(const std::string &fieldPath, const std::vector<uint8_t> &value)
    {
        auto r = setBinary(std::nothrow, fieldPath, value);
        if (!r.has_value())
        {
            throw r.error();
        }
    }

    void MongoDBDocument::setDocument(const std::string &fieldPath, std::shared_ptr<DocumentDBData> doc)
    {
        auto r = setDocument(std::nothrow, fieldPath, std::move(doc));
        if (!r.has_value())
        {
            throw r.error();
        }
    }

    void MongoDBDocument::setNull(const std::string &fieldPath)
    {
        auto r = setNull(std::nothrow, fieldPath);
        if (!r.has_value())
        {
            throw r.error();
        }
    }
#endif // __cpp_exceptions

} // namespace cpp_dbc::MongoDB

#endif // USE_MONGODB
