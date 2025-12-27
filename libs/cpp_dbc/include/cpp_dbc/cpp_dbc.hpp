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

 @file cpp_dbc.hpp
 @brief Main header for the cpp_dbc library - Multi-paradigm database connectivity

*/

#ifndef CPP_DBC_HPP
#define CPP_DBC_HPP

// Configuration macros for conditional compilation
#ifndef USE_MYSQL
#define USE_MYSQL 1 // Default to enabled
#endif

#ifndef USE_POSTGRESQL
#define USE_POSTGRESQL 1 // Default to enabled
#endif

#ifndef USE_SQLITE
#define USE_SQLITE 0 // Default to disabled
#endif

#include <string>
#include <vector>
#include <memory>
#include <map>
#include <stdexcept>
#include <cstdint>
#include <any>

// Core headers
#include "cpp_dbc/core/db_types.hpp"
#include "cpp_dbc/core/db_exception.hpp"
#include "cpp_dbc/core/db_result_set.hpp"
#include "cpp_dbc/core/db_connection.hpp"
#include "cpp_dbc/core/db_driver.hpp"

// Relational database headers
#include "cpp_dbc/core/relational/relational_db_result_set.hpp"
#include "cpp_dbc/core/relational/relational_db_prepared_statement.hpp"
#include "cpp_dbc/core/relational/relational_db_connection.hpp"
#include "cpp_dbc/core/relational/relational_db_driver.hpp"

// Common utilities
#include "cpp_dbc/backward.hpp"
#include "cpp_dbc/common/system_utils.hpp"

// Forward declaration of configuration classes
namespace cpp_dbc
{
    namespace config
    {
        class DatabaseConfig;
        class DatabaseConfigManager;
    }
}

namespace cpp_dbc
{

    // Forward declarations for stream classes
    class InputStream;
    class OutputStream;
    class Blob;

    // Abstract base class for input streams
    class InputStream
    {
    public:
        virtual ~InputStream() = default;

        // Read up to length bytes into the buffer
        // Returns the number of bytes actually read, or -1 if end of stream
        virtual int read(uint8_t *buffer, size_t length) = 0;

        // Skip n bytes
        virtual void skip(size_t n) = 0;

        // Close the stream
        virtual void close() = 0;
    };

    // Abstract base class for output streams
    class OutputStream
    {
    public:
        virtual ~OutputStream() = default;

        // Write length bytes from the buffer
        virtual void write(const uint8_t *buffer, size_t length) = 0;

        // Flush any buffered data
        virtual void flush() = 0;

        // Close the stream
        virtual void close() = 0;
    };

    // Abstract base class for BLOB objects
    class Blob
    {
    public:
        virtual ~Blob() = default;

        // Get the length of the BLOB
        virtual size_t length() const = 0;

        // Get a portion of the BLOB as a vector of bytes
        virtual std::vector<uint8_t> getBytes(size_t pos, size_t length) const = 0;

        // Get a stream to read from the BLOB
        virtual std::shared_ptr<InputStream> getBinaryStream() const = 0;

        // Get a stream to write to the BLOB starting at position pos
        virtual std::shared_ptr<OutputStream> setBinaryStream(size_t pos) = 0;

        // Write bytes to the BLOB starting at position pos
        virtual void setBytes(size_t pos, const std::vector<uint8_t> &bytes) = 0;

        // Write bytes to the BLOB starting at position pos
        virtual void setBytes(size_t pos, const uint8_t *bytes, size_t length) = 0;

        // Truncate the BLOB to the specified length
        virtual void truncate(size_t len) = 0;

        // Free resources associated with the BLOB
        virtual void free() = 0;
    };

    // Manager class to retrieve driver instances
    class DriverManager
    {
    private:
        static std::map<std::string, std::shared_ptr<DBDriver>> drivers;

    public:
        static void registerDriver(const std::string &name, std::shared_ptr<DBDriver> driver);

        // Generic connection method - returns base DBConnection
        static std::shared_ptr<DBConnection> getDBConnection(const std::string &url,
                                                             const std::string &user,
                                                             const std::string &password,
                                                             const std::map<std::string, std::string> &options = std::map<std::string, std::string>());

        // Methods for integration with configuration classes
        static std::shared_ptr<DBConnection> getDBConnection(const config::DatabaseConfig &dbConfig);

        static std::shared_ptr<DBConnection> getDBConnection(const config::DatabaseConfigManager &configManager,
                                                             const std::string &configName);

        // Get list of registered driver names
        static std::vector<std::string> getRegisteredDrivers();

        // Clear all registered drivers (useful for testing)
        static void clearDrivers();

        // Unregister a specific driver
        static void unregisterDriver(const std::string &name);
    };

} // namespace cpp_dbc

#endif // CPP_DBC_HPP
