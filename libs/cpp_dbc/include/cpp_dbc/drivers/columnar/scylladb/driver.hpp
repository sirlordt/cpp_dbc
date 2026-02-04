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
 * @file driver.hpp
 * @brief ScyllaDB driver class (real and stub implementations)
 */

#pragma once

#include "../../../cpp_dbc.hpp"

#include <map>
#include <string>
#include <memory>

#include "cpp_dbc/core/columnar/columnar_db_driver.hpp"
#include "cpp_dbc/core/columnar/columnar_db_connection.hpp"
#include "cpp_dbc/core/db_exception.hpp"

#if USE_SCYLLADB

namespace cpp_dbc::ScyllaDB
{
        class ScyllaDBDriver final : public cpp_dbc::ColumnarDBDriver
        {
        public:
            ScyllaDBDriver();
            ~ScyllaDBDriver() override = default;

            std::shared_ptr<ColumnarDBConnection> connectColumnar(
                const std::string &url,
                const std::string &user,
                const std::string &password,
                const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) override;

            int getDefaultPort() const override;
            std::string getURIScheme() const override;
            std::map<std::string, std::string> parseURI(const std::string &uri) override;
            std::string buildURI(const std::string &host, int port, const std::string &database, const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) override;
            bool supportsClustering() const override;
            bool supportsAsync() const override;
            std::string getDriverVersion() const override;

            // Nothrow API
            cpp_dbc::expected<std::shared_ptr<ColumnarDBConnection>, DBException> connectColumnar(
                std::nothrow_t,
                const std::string &url,
                const std::string &user,
                const std::string &password,
                const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) noexcept override;

            cpp_dbc::expected<std::map<std::string, std::string>, DBException> parseURI(std::nothrow_t, const std::string &uri) noexcept override;

            bool acceptsURL(const std::string &url) override;
            std::string getName() const noexcept override;
        };
} // namespace cpp_dbc::ScyllaDB

#else // USE_SCYLLADB

namespace cpp_dbc::ScyllaDB
{
    class ScyllaDBDriver final : public ColumnarDBDriver
    {
    public:
        [[noreturn]] ScyllaDBDriver() { throw DBException("5F7826C0D4F2", "ScyllaDB support is not enabled in this build"); }
        ~ScyllaDBDriver() override = default;

        ScyllaDBDriver(const ScyllaDBDriver &) = delete;
        ScyllaDBDriver &operator=(const ScyllaDBDriver &) = delete;
        ScyllaDBDriver(ScyllaDBDriver &&) = delete;
        ScyllaDBDriver &operator=(ScyllaDBDriver &&) = delete;

        [[noreturn]] std::shared_ptr<ColumnarDBConnection> connectColumnar(const std::string &, const std::string &, const std::string &, const std::map<std::string, std::string> & = std::map<std::string, std::string>()) override
        {
            throw DBException("C0414E6FE88D", "ScyllaDB support is not enabled in this build");
        }
        int getDefaultPort() const override { return 9042; }
        std::string getURIScheme() const override { return "scylladb"; }
        [[noreturn]] std::map<std::string, std::string> parseURI(const std::string &) override { throw DBException("DWTW9M4YLQ1F", "ScyllaDB support is not enabled in this build"); }
        [[noreturn]] std::string buildURI(const std::string &, int, const std::string &, const std::map<std::string, std::string> & = std::map<std::string, std::string>()) override { throw DBException("1GBAFHO4GP0V", "ScyllaDB support is not enabled in this build"); }
        bool supportsClustering() const override { return false; }
        bool supportsAsync() const override { return false; }
        std::string getDriverVersion() const override { return "0.0.0"; }

        cpp_dbc::expected<std::shared_ptr<ColumnarDBConnection>, DBException> connectColumnar(std::nothrow_t, const std::string &, const std::string &, const std::string &, const std::map<std::string, std::string> & = std::map<std::string, std::string>()) noexcept override
        {
            return cpp_dbc::unexpected(DBException("J48UI1C472FF", "ScyllaDB support is not enabled in this build"));
        }
        cpp_dbc::expected<std::map<std::string, std::string>, DBException> parseURI(std::nothrow_t, const std::string &) noexcept override
        {
            return cpp_dbc::unexpected(DBException("7PPDEW842J3I", "ScyllaDB support is not enabled in this build"));
        }
        bool acceptsURL(const std::string &) override { return false; }
        std::string getName() const noexcept override { return "scylladb"; }
    };
} // namespace cpp_dbc::ScyllaDB

#endif // USE_SCYLLADB
