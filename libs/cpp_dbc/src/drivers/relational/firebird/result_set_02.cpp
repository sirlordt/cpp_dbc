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

 * This file is part of the cpp_dbc project and is licensed under the GNU GPL v3.
 * See the LICENSE.md file in the project root for more information.

 @file result_set_02.cpp
 @brief Firebird database driver implementation - Part 2: FirebirdDBResultSet throwing methods

*/

#include "cpp_dbc/drivers/relational/driver_firebird.hpp"

#if USE_FIREBIRD

#include <sstream>
#include <algorithm>
#include <chrono>
#include <thread>
#include <cstring>
#include <cstdlib>
#include <cmath>

#include "firebird_internal.hpp"

namespace cpp_dbc::Firebird
{

#ifdef __cpp_exceptions
    bool FirebirdDBResultSet::next()
    {
        auto result = next(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    bool FirebirdDBResultSet::isBeforeFirst()
    {
        auto result = isBeforeFirst(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    bool FirebirdDBResultSet::isAfterLast()
    {
        auto result = isAfterLast(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    uint64_t FirebirdDBResultSet::getRow()
    {
        auto result = getRow(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    int FirebirdDBResultSet::getInt(size_t columnIndex)
    {
        auto result = getInt(std::nothrow, columnIndex);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    int FirebirdDBResultSet::getInt(const std::string &columnName)
    {
        auto result = getInt(std::nothrow, columnName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    int64_t FirebirdDBResultSet::getLong(size_t columnIndex)
    {
        auto result = getLong(std::nothrow, columnIndex);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    int64_t FirebirdDBResultSet::getLong(const std::string &columnName)
    {
        auto result = getLong(std::nothrow, columnName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    double FirebirdDBResultSet::getDouble(size_t columnIndex)
    {
        auto result = getDouble(std::nothrow, columnIndex);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    double FirebirdDBResultSet::getDouble(const std::string &columnName)
    {
        auto result = getDouble(std::nothrow, columnName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::string FirebirdDBResultSet::getString(size_t columnIndex)
    {
        auto result = getString(std::nothrow, columnIndex);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::string FirebirdDBResultSet::getString(const std::string &columnName)
    {
        auto result = getString(std::nothrow, columnName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    bool FirebirdDBResultSet::getBoolean(size_t columnIndex)
    {
        auto result = getBoolean(std::nothrow, columnIndex);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    bool FirebirdDBResultSet::getBoolean(const std::string &columnName)
    {
        auto result = getBoolean(std::nothrow, columnName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    bool FirebirdDBResultSet::isNull(size_t columnIndex)
    {
        auto result = isNull(std::nothrow, columnIndex);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    bool FirebirdDBResultSet::isNull(const std::string &columnName)
    {
        auto result = isNull(std::nothrow, columnName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::string FirebirdDBResultSet::getDate(size_t columnIndex)
    {
        auto result = getDate(std::nothrow, columnIndex);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::string FirebirdDBResultSet::getDate(const std::string &columnName)
    {
        auto result = getDate(std::nothrow, columnName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::string FirebirdDBResultSet::getTimestamp(size_t columnIndex)
    {
        auto result = getTimestamp(std::nothrow, columnIndex);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::string FirebirdDBResultSet::getTimestamp(const std::string &columnName)
    {
        auto result = getTimestamp(std::nothrow, columnName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::string FirebirdDBResultSet::getTime(size_t columnIndex)
    {
        auto result = getTime(std::nothrow, columnIndex);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::string FirebirdDBResultSet::getTime(const std::string &columnName)
    {
        auto result = getTime(std::nothrow, columnName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::vector<std::string> FirebirdDBResultSet::getColumnNames()
    {
        auto result = getColumnNames(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    size_t FirebirdDBResultSet::getColumnCount()
    {
        auto result = getColumnCount(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    void FirebirdDBResultSet::close()
    {
        auto result = close(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    bool FirebirdDBResultSet::isEmpty()
    {
        auto result = isEmpty(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::shared_ptr<Blob> FirebirdDBResultSet::getBlob(size_t columnIndex)
    {
        auto result = getBlob(std::nothrow, columnIndex);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::shared_ptr<Blob> FirebirdDBResultSet::getBlob(const std::string &columnName)
    {
        auto result = getBlob(std::nothrow, columnName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::shared_ptr<InputStream> FirebirdDBResultSet::getBinaryStream(size_t columnIndex)
    {
        auto result = getBinaryStream(std::nothrow, columnIndex);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::shared_ptr<InputStream> FirebirdDBResultSet::getBinaryStream(const std::string &columnName)
    {
        auto result = getBinaryStream(std::nothrow, columnName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::vector<uint8_t> FirebirdDBResultSet::getBytes(size_t columnIndex)
    {
        auto result = getBytes(std::nothrow, columnIndex);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::vector<uint8_t> FirebirdDBResultSet::getBytes(const std::string &columnName)
    {
        auto result = getBytes(std::nothrow, columnName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }
#endif // __cpp_exceptions

} // namespace cpp_dbc::Firebird

#endif // USE_FIREBIRD
