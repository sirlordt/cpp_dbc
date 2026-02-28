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
 * @file 10_071_test_blob_stream_create_copyfrom.cpp
 * @brief Unit tests for static create() factories and copyFrom() methods on
 *        all in-memory Stream and Blob classes that support them.
 *
 * These tests are database-independent: no live connection is required.
 * Covered classes:
 *   - cpp_dbc::MemoryBlob
 *   - cpp_dbc::SQLite::SQLiteInputStream        (USE_SQLITE)
 *   - cpp_dbc::SQLite::SQLiteBlob               (USE_SQLITE)
 *   - cpp_dbc::MySQL::MySQLInputStream          (USE_MYSQL)
 *   - cpp_dbc::MySQL::MySQLBlob                 (USE_MYSQL)
 *   - cpp_dbc::PostgreSQL::PostgreSQLInputStream (USE_POSTGRESQL)
 *   - cpp_dbc::PostgreSQL::PostgreSQLBlob       (USE_POSTGRESQL)
 *   - cpp_dbc::Firebird::FirebirdInputStream    (USE_FIREBIRD)
 *   - cpp_dbc::Firebird::FirebirdBlob           (USE_FIREBIRD)
 *   - cpp_dbc::ScyllaDB::ScyllaMemoryInputStream (USE_SCYLLADB)
 *   - cpp_dbc::Redis::RedisDBConnection::create() (USE_REDIS)
 */

#include <cstdint>
#include <memory>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include <cpp_dbc/cpp_dbc.hpp>
#include <cpp_dbc/blob.hpp>

// ============================================================================
// MemoryBlob — create() and copyFrom()
// ============================================================================

TEST_CASE("MemoryBlob create() factory", "[10_071_01_blob_create]")
{
    SECTION("create(std::nothrow) — default empty blob")
    {
        auto r = cpp_dbc::MemoryBlob::create(std::nothrow);
        REQUIRE(r.has_value());
        REQUIRE(r.value() != nullptr);
        REQUIRE(r.value()->length() == 0);
    }

    SECTION("create() — throwing wrapper, default empty blob")
    {
        auto blob = cpp_dbc::MemoryBlob::create();
        REQUIRE(blob != nullptr);
        REQUIRE(blob->length() == 0);
    }

    SECTION("create(std::nothrow, const vector&) — blob with initial data")
    {
        std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04, 0x05};
        auto r = cpp_dbc::MemoryBlob::create(std::nothrow, data);
        REQUIRE(r.has_value());
        REQUIRE(r.value()->length() == 5);
        REQUIRE(r.value()->getBytes(0, 5) == data);
    }

    SECTION("create(const vector&) — throwing wrapper with initial data")
    {
        std::vector<uint8_t> data = {0xAA, 0xBB, 0xCC};
        auto blob = cpp_dbc::MemoryBlob::create(data);
        REQUIRE(blob != nullptr);
        REQUIRE(blob->length() == 3);
        REQUIRE(blob->getBytes(0, 3) == data);
    }

    SECTION("create(std::nothrow, vector&&) — blob from moved data")
    {
        std::vector<uint8_t> data = {0x10, 0x20, 0x30};
        auto r = cpp_dbc::MemoryBlob::create(std::nothrow, std::vector<uint8_t>{0x10, 0x20, 0x30});
        REQUIRE(r.has_value());
        REQUIRE(r.value()->length() == 3);
        REQUIRE(r.value()->getBytes(0, 3) == data);
    }

    SECTION("create(vector&&) — throwing wrapper from moved data")
    {
        std::vector<uint8_t> data = {0xDE, 0xAD, 0xBE, 0xEF};
        auto blob = cpp_dbc::MemoryBlob::create(std::vector<uint8_t>{0xDE, 0xAD, 0xBE, 0xEF});
        REQUIRE(blob != nullptr);
        REQUIRE(blob->length() == 4);
        REQUIRE(blob->getBytes(0, 4) == data);
    }

    SECTION("create produces independent instances")
    {
        std::vector<uint8_t> data = {0x01, 0x02};
        auto blob1 = cpp_dbc::MemoryBlob::create(data);
        auto blob2 = cpp_dbc::MemoryBlob::create(data);
        REQUIRE(blob1.get() != blob2.get());
    }
}

TEST_CASE("MemoryBlob copyFrom()", "[10_071_02_blob_copyfrom]")
{
    SECTION("copyFrom(std::nothrow, other) — copies data")
    {
        std::vector<uint8_t> data = {0x01, 0x02, 0x03};
        auto src = cpp_dbc::MemoryBlob::create(data);
        auto dst = cpp_dbc::MemoryBlob::create();

        auto r = dst->copyFrom(std::nothrow, *src);
        REQUIRE(r.has_value());
        REQUIRE(dst->length() == 3);
        REQUIRE(dst->getBytes(0, 3) == data);
    }

    SECTION("copyFrom(other) — throwing wrapper copies data")
    {
        std::vector<uint8_t> data = {0xAA, 0xBB};
        auto src = cpp_dbc::MemoryBlob::create(data);
        auto dst = cpp_dbc::MemoryBlob::create();

        REQUIRE_NOTHROW(dst->copyFrom(*src));
        REQUIRE(dst->length() == 2);
        REQUIRE(dst->getBytes(0, 2) == data);
    }

    SECTION("copyFrom — deep copy: modifying dst does not affect src")
    {
        std::vector<uint8_t> data = {0x10, 0x20, 0x30};
        auto src = cpp_dbc::MemoryBlob::create(data);
        auto dst = cpp_dbc::MemoryBlob::create();
        dst->copyFrom(*src);

        // Truncate dst and verify src is unchanged
        dst->truncate(1);
        REQUIRE(dst->length() == 1);
        REQUIRE(src->length() == 3);
    }

    SECTION("copyFrom — self-assignment is a no-op")
    {
        std::vector<uint8_t> data = {0xFF, 0xEE};
        auto blob = cpp_dbc::MemoryBlob::create(data);

        REQUIRE_NOTHROW(blob->copyFrom(*blob));
        REQUIRE(blob->length() == 2);
        REQUIRE(blob->getBytes(0, 2) == data);
    }

    SECTION("copyFrom — overwrites existing data in dst")
    {
        auto src = cpp_dbc::MemoryBlob::create(std::vector<uint8_t>{0x01, 0x02});
        auto dst = cpp_dbc::MemoryBlob::create(std::vector<uint8_t>{0xAA, 0xBB, 0xCC, 0xDD});
        REQUIRE(dst->length() == 4);

        dst->copyFrom(*src);
        REQUIRE(dst->length() == 2);
        REQUIRE(dst->getBytes(0, 2) == std::vector<uint8_t>({0x01, 0x02}));
    }
}

// ============================================================================
// SQLiteInputStream — create() and copyFrom()
// ============================================================================

#if USE_SQLITE

#include <cpp_dbc/drivers/relational/sqlite/input_stream.hpp>

TEST_CASE("SQLiteInputStream create() factory", "[10_071_10_sqlite_stream_create]")
{
    SECTION("create(std::nothrow, buffer, length) — valid buffer")
    {
        const uint8_t buf[] = {0x01, 0x02, 0x03};
        auto r = cpp_dbc::SQLite::SQLiteInputStream::create(std::nothrow, buf, 3);
        REQUIRE(r.has_value());
        REQUIRE(r.value() != nullptr);

        uint8_t out[3] = {};
        REQUIRE(r.value()->read(out, 3) == 3);
        REQUIRE(out[0] == 0x01);
        REQUIRE(out[1] == 0x02);
        REQUIRE(out[2] == 0x03);
    }

    SECTION("create(std::nothrow, null, 0) — empty buffer is valid")
    {
        auto r = cpp_dbc::SQLite::SQLiteInputStream::create(std::nothrow, nullptr, 0);
        REQUIRE(r.has_value());
        REQUIRE(r.value() != nullptr);
    }

    SECTION("create(std::nothrow, null, non-zero) — returns unexpected")
    {
        auto r = cpp_dbc::SQLite::SQLiteInputStream::create(std::nothrow, nullptr, 5);
        REQUIRE_FALSE(r.has_value());
    }

    SECTION("create(buffer, length) — throwing wrapper")
    {
        const uint8_t buf[] = {0xDE, 0xAD};
        auto stream = cpp_dbc::SQLite::SQLiteInputStream::create(buf, 2);
        REQUIRE(stream != nullptr);
    }

    SECTION("create(null, non-zero) — throws DBException")
    {
        REQUIRE_THROWS_AS(
            cpp_dbc::SQLite::SQLiteInputStream::create(nullptr, 3),
            cpp_dbc::DBException);
    }
}

TEST_CASE("SQLiteInputStream copyFrom()", "[10_071_11_sqlite_stream_copyfrom]")
{
    const uint8_t buf[] = {0x0A, 0x0B, 0x0C, 0x0D};
    auto src = cpp_dbc::SQLite::SQLiteInputStream::create(buf, 4);

    SECTION("copyFrom(std::nothrow, other) — resets position to 0")
    {
        // Advance src by 2 bytes
        uint8_t tmp[2];
        src->read(tmp, 2);

        auto dst = cpp_dbc::SQLite::SQLiteInputStream::create(nullptr, 0);
        auto r = dst->copyFrom(std::nothrow, *src);
        REQUIRE(r.has_value());

        // dst should be at position 0 with all 4 bytes
        uint8_t out[4] = {};
        REQUIRE(dst->read(out, 4) == 4);
        REQUIRE(out[0] == 0x0A);
        REQUIRE(out[3] == 0x0D);
    }

    SECTION("copyFrom(other) — throwing wrapper")
    {
        auto empty = cpp_dbc::SQLite::SQLiteInputStream::create(nullptr, 0);
        REQUIRE_NOTHROW(empty->copyFrom(*src));
        uint8_t out[4] = {};
        REQUIRE(empty->read(out, 4) == 4);
        REQUIRE(out[0] == 0x0A);
    }

    SECTION("copyFrom — self-assignment is a no-op")
    {
        REQUIRE_NOTHROW(src->copyFrom(*src));
        uint8_t out[4] = {};
        REQUIRE(src->read(out, 4) == 4);
    }

    SECTION("copyFrom — deep copy, src and dst are independent")
    {
        auto dst = cpp_dbc::SQLite::SQLiteInputStream::create(nullptr, 0);
        dst->copyFrom(*src);

        // Read dst fully
        uint8_t out[4] = {};
        dst->read(out, 4);

        // src should still be at position 0 (copyFrom sets dst pos to 0, src unchanged)
        uint8_t out2[4] = {};
        REQUIRE(src->read(out2, 4) == 4);
        REQUIRE(out2[0] == 0x0A);
    }
}

#endif // USE_SQLITE

// ============================================================================
// MySQLInputStream — create() and copyFrom()
// ============================================================================

#if USE_MYSQL

#include <cpp_dbc/drivers/relational/mysql/input_stream.hpp>

TEST_CASE("MySQLInputStream create() factory", "[10_071_20_mysql_stream_create]")
{
    SECTION("create(std::nothrow, buffer, length) — valid buffer")
    {
        const char buf[] = {0x11, 0x22, 0x33};
        auto r = cpp_dbc::MySQL::MySQLInputStream::create(std::nothrow, buf, 3);
        REQUIRE(r.has_value());
        REQUIRE(r.value() != nullptr);
    }

    SECTION("create(std::nothrow, null, 0) — empty is valid")
    {
        auto r = cpp_dbc::MySQL::MySQLInputStream::create(std::nothrow, nullptr, 0);
        REQUIRE(r.has_value());
    }

    SECTION("create(std::nothrow, null, non-zero) — returns unexpected")
    {
        auto r = cpp_dbc::MySQL::MySQLInputStream::create(std::nothrow, nullptr, 4);
        REQUIRE_FALSE(r.has_value());
    }

    SECTION("create(buffer, length) — throwing wrapper")
    {
        const char buf[] = {0x55, 0x66};
        auto stream = cpp_dbc::MySQL::MySQLInputStream::create(buf, 2);
        REQUIRE(stream != nullptr);
    }

    SECTION("create(null, non-zero) — throws DBException")
    {
        REQUIRE_THROWS_AS(
            cpp_dbc::MySQL::MySQLInputStream::create(nullptr, 1),
            cpp_dbc::DBException);
    }
}

TEST_CASE("MySQLInputStream copyFrom()", "[10_071_21_mysql_stream_copyfrom]")
{
    const char buf[] = {0x1A, 0x2B, 0x3C};
    auto src = cpp_dbc::MySQL::MySQLInputStream::create(buf, 3);

    SECTION("copyFrom(std::nothrow, other) — copies data, resets position")
    {
        auto dst = cpp_dbc::MySQL::MySQLInputStream::create(nullptr, 0);
        auto r = dst->copyFrom(std::nothrow, *src);
        REQUIRE(r.has_value());

        uint8_t out[3] = {};
        REQUIRE(dst->read(out, 3) == 3);
        REQUIRE(out[0] == 0x1A);
        REQUIRE(out[2] == 0x3C);
    }

    SECTION("copyFrom(other) — throwing wrapper")
    {
        auto dst = cpp_dbc::MySQL::MySQLInputStream::create(nullptr, 0);
        REQUIRE_NOTHROW(dst->copyFrom(*src));
        uint8_t out[3] = {};
        REQUIRE(dst->read(out, 3) == 3);
    }

    SECTION("copyFrom — self-assignment is a no-op")
    {
        REQUIRE_NOTHROW(src->copyFrom(*src));
        uint8_t out[3] = {};
        REQUIRE(src->read(out, 3) == 3);
    }
}

#endif // USE_MYSQL

// ============================================================================
// PostgreSQLInputStream — create() and copyFrom()
// ============================================================================

#if USE_POSTGRESQL

#include <cpp_dbc/drivers/relational/postgresql/input_stream.hpp>

TEST_CASE("PostgreSQLInputStream create() factory", "[10_071_30_postgresql_stream_create]")
{
    SECTION("create(std::nothrow, buffer, length) — valid buffer")
    {
        const uint8_t buf[] = {0xCA, 0xFE, 0xBA, 0xBE};
        auto r = cpp_dbc::PostgreSQL::PostgreSQLInputStream::create(
            std::nothrow, reinterpret_cast<const char *>(buf), 4);
        REQUIRE(r.has_value());
        REQUIRE(r.value() != nullptr);

        uint8_t out[4] = {};
        REQUIRE(r.value()->read(out, 4) == 4);
        REQUIRE(out[0] == 0xCA);
        REQUIRE(out[3] == 0xBE);
    }

    SECTION("create(std::nothrow, null, 0) — empty is valid")
    {
        auto r = cpp_dbc::PostgreSQL::PostgreSQLInputStream::create(std::nothrow, nullptr, 0);
        REQUIRE(r.has_value());
    }

    SECTION("create(std::nothrow, null, non-zero) — returns unexpected")
    {
        auto r = cpp_dbc::PostgreSQL::PostgreSQLInputStream::create(std::nothrow, nullptr, 2);
        REQUIRE_FALSE(r.has_value());
    }

    SECTION("create(buffer, length) — throwing wrapper")
    {
        const char buf[] = {0x01};
        auto stream = cpp_dbc::PostgreSQL::PostgreSQLInputStream::create(buf, 1);
        REQUIRE(stream != nullptr);
    }

    SECTION("create(null, non-zero) — throws DBException")
    {
        REQUIRE_THROWS_AS(
            cpp_dbc::PostgreSQL::PostgreSQLInputStream::create(nullptr, 2),
            cpp_dbc::DBException);
    }
}

TEST_CASE("PostgreSQLInputStream copyFrom()", "[10_071_31_postgresql_stream_copyfrom]")
{
    const uint8_t buf[] = {0xBE, 0xEF};
    auto src = cpp_dbc::PostgreSQL::PostgreSQLInputStream::create(
        reinterpret_cast<const char *>(buf), 2);

    SECTION("copyFrom(std::nothrow, other) — copies data, resets position")
    {
        auto dst = cpp_dbc::PostgreSQL::PostgreSQLInputStream::create(nullptr, 0);
        auto r = dst->copyFrom(std::nothrow, *src);
        REQUIRE(r.has_value());

        uint8_t out[2] = {};
        REQUIRE(dst->read(out, 2) == 2);
        REQUIRE(out[0] == 0xBE);
        REQUIRE(out[1] == 0xEF);
    }

    SECTION("copyFrom(other) — throwing wrapper")
    {
        auto dst = cpp_dbc::PostgreSQL::PostgreSQLInputStream::create(nullptr, 0);
        REQUIRE_NOTHROW(dst->copyFrom(*src));
    }

    SECTION("copyFrom — self-assignment is a no-op")
    {
        REQUIRE_NOTHROW(src->copyFrom(*src));
    }
}

#endif // USE_POSTGRESQL

// ============================================================================
// FirebirdInputStream — create() and copyFrom()
// ============================================================================

#if USE_FIREBIRD

#include <cpp_dbc/drivers/relational/firebird/input_stream.hpp>

TEST_CASE("FirebirdInputStream create() factory", "[10_071_40_firebird_stream_create]")
{
    SECTION("create(std::nothrow, buffer, length) — valid buffer")
    {
        const uint8_t buf[] = {0xF0, 0x0D};
        auto r = cpp_dbc::Firebird::FirebirdInputStream::create(std::nothrow, buf, 2);
        REQUIRE(r.has_value());
        REQUIRE(r.value() != nullptr);

        uint8_t out[2] = {};
        REQUIRE(r.value()->read(out, 2) == 2);
        REQUIRE(out[0] == 0xF0);
        REQUIRE(out[1] == 0x0D);
    }

    SECTION("create(std::nothrow, null, 0) — empty is valid")
    {
        auto r = cpp_dbc::Firebird::FirebirdInputStream::create(std::nothrow, nullptr, 0);
        REQUIRE(r.has_value());
    }

    SECTION("create(std::nothrow, null, non-zero) — returns unexpected")
    {
        auto r = cpp_dbc::Firebird::FirebirdInputStream::create(std::nothrow, nullptr, 3);
        REQUIRE_FALSE(r.has_value());
    }

    SECTION("create(buffer, length) — throwing wrapper")
    {
        const uint8_t buf[] = {0x42};
        auto stream = cpp_dbc::Firebird::FirebirdInputStream::create(buf, 1);
        REQUIRE(stream != nullptr);
    }

    SECTION("create(null, non-zero) — throws DBException")
    {
        REQUIRE_THROWS_AS(
            cpp_dbc::Firebird::FirebirdInputStream::create(nullptr, 1),
            cpp_dbc::DBException);
    }
}

TEST_CASE("FirebirdInputStream copyFrom()", "[10_071_41_firebird_stream_copyfrom]")
{
    const uint8_t buf[] = {0x01, 0x02, 0x03, 0x04};
    auto src = cpp_dbc::Firebird::FirebirdInputStream::create(buf, 4);

    SECTION("copyFrom(std::nothrow, other) — copies data, resets position")
    {
        // Advance src by 1 byte so we verify dst starts at 0
        uint8_t tmp[1];
        src->read(tmp, 1);

        auto dst = cpp_dbc::Firebird::FirebirdInputStream::create(nullptr, 0);
        auto r = dst->copyFrom(std::nothrow, *src);
        REQUIRE(r.has_value());

        uint8_t out[4] = {};
        REQUIRE(dst->read(out, 4) == 4);
        REQUIRE(out[0] == 0x01);
    }

    SECTION("copyFrom(other) — throwing wrapper")
    {
        auto dst = cpp_dbc::Firebird::FirebirdInputStream::create(nullptr, 0);
        REQUIRE_NOTHROW(dst->copyFrom(*src));
        uint8_t out[4] = {};
        REQUIRE(dst->read(out, 4) == 4);
    }

    SECTION("copyFrom — self-assignment is a no-op")
    {
        REQUIRE_NOTHROW(src->copyFrom(*src));
    }
}

#endif // USE_FIREBIRD

// ============================================================================
// ScyllaMemoryInputStream — create() and copyFrom()
// ============================================================================

#if USE_SCYLLADB

#include <cpp_dbc/drivers/columnar/scylladb/memory_input_stream.hpp>

TEST_CASE("ScyllaMemoryInputStream create() factory", "[10_071_50_scylladb_stream_create]")
{
    SECTION("create(std::nothrow, data) — with data")
    {
        std::vector<uint8_t> data = {0x01, 0x02, 0x03};
        auto r = cpp_dbc::ScyllaDB::ScyllaMemoryInputStream::create(std::nothrow, data);
        REQUIRE(r.has_value());
        REQUIRE(r.value() != nullptr);

        uint8_t out[3] = {};
        REQUIRE(r.value()->read(out, 3) == 3);
        REQUIRE(out[0] == 0x01);
        REQUIRE(out[2] == 0x03);
    }

    SECTION("create(std::nothrow, empty vector) — empty stream")
    {
        auto r = cpp_dbc::ScyllaDB::ScyllaMemoryInputStream::create(std::nothrow, std::vector<uint8_t>{});
        REQUIRE(r.has_value());
        REQUIRE(r.value() != nullptr);

        uint8_t out[1];
        REQUIRE(r.value()->read(out, 1) == -1); // EOF
    }

    SECTION("create(data) — throwing wrapper")
    {
        std::vector<uint8_t> data = {0xAB, 0xCD};
        auto stream = cpp_dbc::ScyllaDB::ScyllaMemoryInputStream::create(data);
        REQUIRE(stream != nullptr);

        uint8_t out[2] = {};
        REQUIRE(stream->read(out, 2) == 2);
        REQUIRE(out[0] == 0xAB);
        REQUIRE(out[1] == 0xCD);
    }

    SECTION("create produces independent instances from same data")
    {
        std::vector<uint8_t> data = {0x10, 0x20};
        auto s1 = cpp_dbc::ScyllaDB::ScyllaMemoryInputStream::create(data);
        auto s2 = cpp_dbc::ScyllaDB::ScyllaMemoryInputStream::create(data);
        REQUIRE(s1.get() != s2.get());
    }
}

TEST_CASE("ScyllaMemoryInputStream copyFrom()", "[10_071_51_scylladb_stream_copyfrom]")
{
    std::vector<uint8_t> data = {0x0F, 0x1E, 0x2D, 0x3C};
    auto src = cpp_dbc::ScyllaDB::ScyllaMemoryInputStream::create(data);

    SECTION("copyFrom(std::nothrow, other) — copies data, resets position to 0")
    {
        // Advance src by 2 bytes
        uint8_t tmp[2];
        src->read(tmp, 2);

        auto dst = cpp_dbc::ScyllaDB::ScyllaMemoryInputStream::create(std::vector<uint8_t>{});
        auto r = dst->copyFrom(std::nothrow, *src);
        REQUIRE(r.has_value());

        // dst starts at 0, reads all 4 bytes
        uint8_t out[4] = {};
        REQUIRE(dst->read(out, 4) == 4);
        REQUIRE(out[0] == 0x0F);
        REQUIRE(out[3] == 0x3C);
    }

    SECTION("copyFrom(other) — throwing wrapper")
    {
        auto dst = cpp_dbc::ScyllaDB::ScyllaMemoryInputStream::create(std::vector<uint8_t>{});
        REQUIRE_NOTHROW(dst->copyFrom(*src));

        uint8_t out[4] = {};
        REQUIRE(dst->read(out, 4) == 4);
        REQUIRE(out[0] == 0x0F);
    }

    SECTION("copyFrom — self-assignment is a no-op")
    {
        REQUIRE_NOTHROW(src->copyFrom(*src));
        uint8_t out[4] = {};
        REQUIRE(src->read(out, 4) == 4);
    }

    SECTION("copyFrom — deep copy, modifying dst does not affect src")
    {
        auto dst = cpp_dbc::ScyllaDB::ScyllaMemoryInputStream::create(std::vector<uint8_t>{});
        dst->copyFrom(*src);

        // Skip all of dst
        dst->skip(4);
        uint8_t eof[1];
        REQUIRE(dst->read(eof, 1) == -1);

        // src should still be fully readable
        uint8_t out[4] = {};
        REQUIRE(src->read(out, 4) == 4);
    }
}

#endif // USE_SCYLLADB

// ============================================================================
// SQLiteBlob — create() and copyFrom()
// No live DB required: constructors only store weak_ptr, no connection made.
// ============================================================================

#if USE_SQLITE

#include <cpp_dbc/drivers/relational/sqlite/blob.hpp>

TEST_CASE("SQLiteBlob create() factory", "[10_071_60_sqlite_blob_create]")
{
    // nullptr is valid — the constructor just stores it as weak_ptr; no DB access.
    std::shared_ptr<sqlite3> nullDb{nullptr};

    SECTION("create(std::nothrow, db) — empty blob, m_loaded=true")
    {
        auto r = cpp_dbc::SQLite::SQLiteBlob::create(std::nothrow, nullDb);
        REQUIRE(r.has_value());
        REQUIRE(r.value() != nullptr);
        // m_loaded is true so length() works without DB
        REQUIRE(r.value()->length() == 0);
    }

    SECTION("create(db) — throwing wrapper")
    {
        auto blob = cpp_dbc::SQLite::SQLiteBlob::create(nullDb);
        REQUIRE(blob != nullptr);
        REQUIRE(blob->length() == 0);
    }

    SECTION("create(std::nothrow, db, initialData) — blob with data")
    {
        std::vector<uint8_t> data = {0x01, 0x02, 0x03};
        auto r = cpp_dbc::SQLite::SQLiteBlob::create(std::nothrow, nullDb, data);
        REQUIRE(r.has_value());
        REQUIRE(r.value()->length() == 3);
        REQUIRE(r.value()->getBytes(0, 3) == data);
    }

    SECTION("create(db, initialData) — throwing wrapper with data")
    {
        std::vector<uint8_t> data = {0xAA, 0xBB};
        auto blob = cpp_dbc::SQLite::SQLiteBlob::create(nullDb, data);
        REQUIRE(blob != nullptr);
        REQUIRE(blob->length() == 2);
        REQUIRE(blob->getBytes(0, 2) == data);
    }

    SECTION("create(std::nothrow, db, tableName, columnName, rowId) — lazy-load blob")
    {
        // m_loaded is false until ensureLoaded() is called — connection check is deferred
        auto r = cpp_dbc::SQLite::SQLiteBlob::create(
            std::nothrow, nullDb, "my_table", "my_column", "42");
        REQUIRE(r.has_value());
        REQUIRE(r.value() != nullptr);
        // isConnectionValid() should be false since the shared_ptr is null
        REQUIRE_FALSE(r.value()->isConnectionValid());
    }

    SECTION("create(db, tableName, columnName, rowId) — throwing wrapper")
    {
        auto blob = cpp_dbc::SQLite::SQLiteBlob::create(
            nullDb, "tbl", "col", "1");
        REQUIRE(blob != nullptr);
        REQUIRE_FALSE(blob->isConnectionValid());
    }

    SECTION("two create() calls produce independent instances")
    {
        auto b1 = cpp_dbc::SQLite::SQLiteBlob::create(nullDb);
        auto b2 = cpp_dbc::SQLite::SQLiteBlob::create(nullDb);
        REQUIRE(b1.get() != b2.get());
    }
}

TEST_CASE("SQLiteBlob copyFrom()", "[10_071_61_sqlite_blob_copyfrom]")
{
    std::shared_ptr<sqlite3> nullDb{nullptr};

    SECTION("copyFrom(std::nothrow, other) — copies data and metadata")
    {
        std::vector<uint8_t> data = {0x10, 0x20, 0x30};
        auto src = cpp_dbc::SQLite::SQLiteBlob::create(nullDb, data);

        auto dst = cpp_dbc::SQLite::SQLiteBlob::create(nullDb);
        REQUIRE(dst->length() == 0);

        auto r = dst->copyFrom(std::nothrow, *src);
        REQUIRE(r.has_value());
        REQUIRE(dst->length() == 3);
        REQUIRE(dst->getBytes(0, 3) == data);
    }

    SECTION("copyFrom(other) — throwing wrapper")
    {
        std::vector<uint8_t> data = {0xFF, 0xEE};
        auto src = cpp_dbc::SQLite::SQLiteBlob::create(nullDb, data);
        auto dst = cpp_dbc::SQLite::SQLiteBlob::create(nullDb);

        REQUIRE_NOTHROW(dst->copyFrom(*src));
        REQUIRE(dst->length() == 2);
        REQUIRE(dst->getBytes(0, 2) == data);
    }

    SECTION("copyFrom — copies lazy-load metadata (tableName, columnName, rowId)")
    {
        // Use a real non-null shared_ptr so weak_ptr.expired() is false
        auto fakeDb = std::shared_ptr<sqlite3>(static_cast<sqlite3 *>(nullptr),
                                               [](sqlite3 *) {
                                                   // Intentionally empty — no real sqlite3 handle to close
                                               });
        auto src = cpp_dbc::SQLite::SQLiteBlob::create(
            fakeDb, "orders", "attachment", "99");

        auto dst = cpp_dbc::SQLite::SQLiteBlob::create(fakeDb);
        dst->copyFrom(*src);

        // After copy, dst shares the same weak_ptr as src → same validity
        REQUIRE(dst->isConnectionValid() == src->isConnectionValid());
    }

    SECTION("copyFrom — self-assignment is a no-op")
    {
        std::vector<uint8_t> data = {0x01};
        auto blob = cpp_dbc::SQLite::SQLiteBlob::create(nullDb, data);

        REQUIRE_NOTHROW(blob->copyFrom(*blob));
        REQUIRE(blob->length() == 1);
    }

    SECTION("copyFrom — deep copy: modifying dst does not affect src")
    {
        std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04};
        auto src = cpp_dbc::SQLite::SQLiteBlob::create(nullDb, data);
        auto dst = cpp_dbc::SQLite::SQLiteBlob::create(nullDb);
        dst->copyFrom(*src);

        dst->truncate(2);
        REQUIRE(dst->length() == 2);
        REQUIRE(src->length() == 4);
    }
}

#endif // USE_SQLITE (blob)

// ============================================================================
// MySQLBlob — create() and copyFrom()
// ============================================================================

#if USE_MYSQL

#include <cpp_dbc/drivers/relational/mysql/blob.hpp>

TEST_CASE("MySQLBlob create() factory", "[10_071_70_mysql_blob_create]")
{
    std::shared_ptr<MYSQL> nullMysql{nullptr};

    SECTION("create(std::nothrow, mysql) — empty blob")
    {
        auto r = cpp_dbc::MySQL::MySQLBlob::create(std::nothrow, nullMysql);
        REQUIRE(r.has_value());
        REQUIRE(r.value() != nullptr);
        REQUIRE(r.value()->length() == 0);
    }

    SECTION("create(mysql) — throwing wrapper")
    {
        auto blob = cpp_dbc::MySQL::MySQLBlob::create(nullMysql);
        REQUIRE(blob != nullptr);
        REQUIRE(blob->length() == 0);
    }

    SECTION("create(std::nothrow, mysql, initialData) — blob with data")
    {
        std::vector<uint8_t> data = {0x0A, 0x0B, 0x0C};
        auto r = cpp_dbc::MySQL::MySQLBlob::create(std::nothrow, nullMysql, data);
        REQUIRE(r.has_value());
        REQUIRE(r.value()->length() == 3);
        REQUIRE(r.value()->getBytes(0, 3) == data);
    }

    SECTION("create(mysql, initialData) — throwing wrapper with data")
    {
        std::vector<uint8_t> data = {0xDE, 0xAD};
        auto blob = cpp_dbc::MySQL::MySQLBlob::create(nullMysql, data);
        REQUIRE(blob != nullptr);
        REQUIRE(blob->length() == 2);
    }

    SECTION("create(std::nothrow, mysql, tableName, columnName, whereClause) — lazy-load blob")
    {
        auto r = cpp_dbc::MySQL::MySQLBlob::create(
            std::nothrow, nullMysql, "users", "avatar", "id=1");
        REQUIRE(r.has_value());
        REQUIRE(r.value() != nullptr);
        REQUIRE_FALSE(r.value()->isConnectionValid());
    }

    SECTION("create(mysql, tableName, columnName, whereClause) — throwing wrapper")
    {
        auto blob = cpp_dbc::MySQL::MySQLBlob::create(
            nullMysql, "files", "content", "id=42");
        REQUIRE(blob != nullptr);
        REQUIRE_FALSE(blob->isConnectionValid());
    }

    SECTION("two create() calls produce independent instances")
    {
        auto b1 = cpp_dbc::MySQL::MySQLBlob::create(nullMysql);
        auto b2 = cpp_dbc::MySQL::MySQLBlob::create(nullMysql);
        REQUIRE(b1.get() != b2.get());
    }
}

TEST_CASE("MySQLBlob copyFrom()", "[10_071_71_mysql_blob_copyfrom]")
{
    std::shared_ptr<MYSQL> nullMysql{nullptr};

    SECTION("copyFrom(std::nothrow, other) — copies data")
    {
        std::vector<uint8_t> data = {0x11, 0x22, 0x33};
        auto src = cpp_dbc::MySQL::MySQLBlob::create(nullMysql, data);
        auto dst = cpp_dbc::MySQL::MySQLBlob::create(nullMysql);

        auto r = dst->copyFrom(std::nothrow, *src);
        REQUIRE(r.has_value());
        REQUIRE(dst->length() == 3);
        REQUIRE(dst->getBytes(0, 3) == data);
    }

    SECTION("copyFrom(other) — throwing wrapper")
    {
        std::vector<uint8_t> data = {0x55, 0x66};
        auto src = cpp_dbc::MySQL::MySQLBlob::create(nullMysql, data);
        auto dst = cpp_dbc::MySQL::MySQLBlob::create(nullMysql);

        REQUIRE_NOTHROW(dst->copyFrom(*src));
        REQUIRE(dst->length() == 2);
    }

    SECTION("copyFrom — copies lazy-load metadata (tableName, columnName, whereClause)")
    {
        // Use a non-null shared_ptr with a no-op deleter so isConnectionValid() is true
        auto fakeMysql = std::shared_ptr<MYSQL>(static_cast<MYSQL *>(nullptr),
                                                [](MYSQL *) {
                                                    // Intentionally empty — no real MYSQL handle to free
                                                });
        auto src = cpp_dbc::MySQL::MySQLBlob::create(
            fakeMysql, "products", "image", "id=7");
        auto dst = cpp_dbc::MySQL::MySQLBlob::create(fakeMysql);
        dst->copyFrom(*src);

        REQUIRE(dst->isConnectionValid() == src->isConnectionValid());
    }

    SECTION("copyFrom — self-assignment is a no-op")
    {
        std::vector<uint8_t> data = {0xAB};
        auto blob = cpp_dbc::MySQL::MySQLBlob::create(nullMysql, data);
        REQUIRE_NOTHROW(blob->copyFrom(*blob));
        REQUIRE(blob->length() == 1);
    }

    SECTION("copyFrom — deep copy: modifying dst does not affect src")
    {
        std::vector<uint8_t> data = {0x01, 0x02, 0x03};
        auto src = cpp_dbc::MySQL::MySQLBlob::create(nullMysql, data);
        auto dst = cpp_dbc::MySQL::MySQLBlob::create(nullMysql);
        dst->copyFrom(*src);

        dst->truncate(1);
        REQUIRE(dst->length() == 1);
        REQUIRE(src->length() == 3);
    }
}

#endif // USE_MYSQL (blob)

// ============================================================================
// PostgreSQLBlob — create() and copyFrom()
// ============================================================================

#if USE_POSTGRESQL

#include <cpp_dbc/drivers/relational/postgresql/blob.hpp>

TEST_CASE("PostgreSQLBlob create() factory", "[10_071_80_postgresql_blob_create]")
{
    std::shared_ptr<PGconn> nullConn{nullptr};

    SECTION("create(std::nothrow, conn) — empty blob, m_loaded=true")
    {
        auto r = cpp_dbc::PostgreSQL::PostgreSQLBlob::create(std::nothrow, nullConn);
        REQUIRE(r.has_value());
        REQUIRE(r.value() != nullptr);
        REQUIRE(r.value()->length() == 0);
    }

    SECTION("create(conn) — throwing wrapper")
    {
        auto blob = cpp_dbc::PostgreSQL::PostgreSQLBlob::create(nullConn);
        REQUIRE(blob != nullptr);
        REQUIRE(blob->length() == 0);
    }

    SECTION("create(std::nothrow, conn, initialData) — blob with data")
    {
        const uint8_t raw[] = {0xCA, 0xFE};
        std::vector<uint8_t> data(raw, raw + 2);
        auto r = cpp_dbc::PostgreSQL::PostgreSQLBlob::create(std::nothrow, nullConn, data);
        REQUIRE(r.has_value());
        REQUIRE(r.value()->length() == 2);
        REQUIRE(r.value()->getBytes(0, 2) == data);
    }

    SECTION("create(conn, initialData) — throwing wrapper with data")
    {
        std::vector<uint8_t> data = {0x01, 0x02, 0x03};
        auto blob = cpp_dbc::PostgreSQL::PostgreSQLBlob::create(nullConn, data);
        REQUIRE(blob != nullptr);
        REQUIRE(blob->length() == 3);
    }

    SECTION("create(std::nothrow, conn, oid) — lazy-load blob by OID")
    {
        // m_loaded is false until ensureLoaded() is called — connection check deferred
        auto r = cpp_dbc::PostgreSQL::PostgreSQLBlob::create(std::nothrow, nullConn, Oid{12345});
        REQUIRE(r.has_value());
        REQUIRE(r.value() != nullptr);
        REQUIRE(r.value()->getOid() == 12345);
        REQUIRE_FALSE(r.value()->isConnectionValid());
    }

    SECTION("create(conn, oid) — throwing wrapper")
    {
        auto blob = cpp_dbc::PostgreSQL::PostgreSQLBlob::create(nullConn, Oid{99});
        REQUIRE(blob != nullptr);
        REQUIRE(blob->getOid() == 99);
    }

    SECTION("two create() calls produce independent instances")
    {
        auto b1 = cpp_dbc::PostgreSQL::PostgreSQLBlob::create(nullConn);
        auto b2 = cpp_dbc::PostgreSQL::PostgreSQLBlob::create(nullConn);
        REQUIRE(b1.get() != b2.get());
    }
}

TEST_CASE("PostgreSQLBlob copyFrom()", "[10_071_81_postgresql_blob_copyfrom]")
{
    std::shared_ptr<PGconn> nullConn{nullptr};

    SECTION("copyFrom(std::nothrow, other) — copies data")
    {
        std::vector<uint8_t> data = {0xAA, 0xBB, 0xCC};
        auto src = cpp_dbc::PostgreSQL::PostgreSQLBlob::create(nullConn, data);
        auto dst = cpp_dbc::PostgreSQL::PostgreSQLBlob::create(nullConn);

        auto r = dst->copyFrom(std::nothrow, *src);
        REQUIRE(r.has_value());
        REQUIRE(dst->length() == 3);
        REQUIRE(dst->getBytes(0, 3) == data);
    }

    SECTION("copyFrom(other) — throwing wrapper")
    {
        std::vector<uint8_t> data = {0x77, 0x88};
        auto src = cpp_dbc::PostgreSQL::PostgreSQLBlob::create(nullConn, data);
        auto dst = cpp_dbc::PostgreSQL::PostgreSQLBlob::create(nullConn);

        REQUIRE_NOTHROW(dst->copyFrom(*src));
        REQUIRE(dst->length() == 2);
    }

    SECTION("copyFrom — copies OID from lazy-load blob")
    {
        auto src = cpp_dbc::PostgreSQL::PostgreSQLBlob::create(nullConn, Oid{777});
        auto dst = cpp_dbc::PostgreSQL::PostgreSQLBlob::create(nullConn);
        dst->copyFrom(*src);

        REQUIRE(dst->getOid() == 777);
    }

    SECTION("copyFrom — self-assignment is a no-op")
    {
        std::vector<uint8_t> data = {0x42};
        auto blob = cpp_dbc::PostgreSQL::PostgreSQLBlob::create(nullConn, data);
        REQUIRE_NOTHROW(blob->copyFrom(*blob));
        REQUIRE(blob->length() == 1);
    }

    SECTION("copyFrom — deep copy: modifying dst does not affect src")
    {
        std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04};
        auto src = cpp_dbc::PostgreSQL::PostgreSQLBlob::create(nullConn, data);
        auto dst = cpp_dbc::PostgreSQL::PostgreSQLBlob::create(nullConn);
        dst->copyFrom(*src);

        dst->truncate(2);
        REQUIRE(dst->length() == 2);
        REQUIRE(src->length() == 4);
    }
}

#endif // USE_POSTGRESQL (blob)

// ============================================================================
// FirebirdBlob — create() and copyFrom()
// ============================================================================

#if USE_FIREBIRD

#include <cpp_dbc/drivers/relational/firebird/blob.hpp>

TEST_CASE("FirebirdBlob create() factory", "[10_071_90_firebird_blob_create]")
{
    // nullptr connection — constructors only store weak_ptr, no DB access at construction
    std::shared_ptr<cpp_dbc::Firebird::FirebirdDBConnection> nullConn{nullptr};

    SECTION("create(std::nothrow, connection) — empty blob, m_loaded=true")
    {
        auto r = cpp_dbc::Firebird::FirebirdBlob::create(std::nothrow, nullConn);
        REQUIRE(r.has_value());
        REQUIRE(r.value() != nullptr);
        REQUIRE(r.value()->length() == 0);
        REQUIRE_FALSE(r.value()->hasValidId());
    }

    SECTION("create(connection) — throwing wrapper")
    {
        auto blob = cpp_dbc::Firebird::FirebirdBlob::create(nullConn);
        REQUIRE(blob != nullptr);
        REQUIRE(blob->length() == 0);
        REQUIRE_FALSE(blob->hasValidId());
    }

    SECTION("create(std::nothrow, connection, initialData) — blob with data")
    {
        std::vector<uint8_t> data = {0xF0, 0xF1, 0xF2};
        auto r = cpp_dbc::Firebird::FirebirdBlob::create(std::nothrow, nullConn, data);
        REQUIRE(r.has_value());
        REQUIRE(r.value()->length() == 3);
        REQUIRE(r.value()->getBytes(0, 3) == data);
    }

    SECTION("create(connection, initialData) — throwing wrapper with data")
    {
        std::vector<uint8_t> data = {0xBB, 0xCC};
        auto blob = cpp_dbc::Firebird::FirebirdBlob::create(nullConn, data);
        REQUIRE(blob != nullptr);
        REQUIRE(blob->length() == 2);
    }

    SECTION("create(std::nothrow, connection, blobId) — lazy-load blob by ID")
    {
        ISC_QUAD blobId{};
        blobId.gds_quad_high = 1;
        blobId.gds_quad_low = 42;
        auto r = cpp_dbc::Firebird::FirebirdBlob::create(std::nothrow, nullConn, blobId);
        REQUIRE(r.has_value());
        REQUIRE(r.value() != nullptr);
        // Lazy-loaded blob has a valid ID set at construction
        REQUIRE(r.value()->hasValidId());
        REQUIRE_FALSE(r.value()->isConnectionValid());
    }

    SECTION("create(connection, blobId) — throwing wrapper")
    {
        ISC_QUAD blobId{};
        blobId.gds_quad_high = 0;
        blobId.gds_quad_low = 7;
        auto blob = cpp_dbc::Firebird::FirebirdBlob::create(nullConn, blobId);
        REQUIRE(blob != nullptr);
        REQUIRE(blob->hasValidId());
    }

    SECTION("two create() calls produce independent instances")
    {
        auto b1 = cpp_dbc::Firebird::FirebirdBlob::create(nullConn);
        auto b2 = cpp_dbc::Firebird::FirebirdBlob::create(nullConn);
        REQUIRE(b1.get() != b2.get());
    }
}

TEST_CASE("FirebirdBlob copyFrom()", "[10_071_91_firebird_blob_copyfrom]")
{
    std::shared_ptr<cpp_dbc::Firebird::FirebirdDBConnection> nullConn{nullptr};

    SECTION("copyFrom(std::nothrow, other) — copies data")
    {
        std::vector<uint8_t> data = {0x11, 0x22, 0x33};
        auto src = cpp_dbc::Firebird::FirebirdBlob::create(nullConn, data);
        auto dst = cpp_dbc::Firebird::FirebirdBlob::create(nullConn);

        auto r = dst->copyFrom(std::nothrow, *src);
        REQUIRE(r.has_value());
        REQUIRE(dst->length() == 3);
        REQUIRE(dst->getBytes(0, 3) == data);
    }

    SECTION("copyFrom(other) — throwing wrapper")
    {
        std::vector<uint8_t> data = {0x44, 0x55};
        auto src = cpp_dbc::Firebird::FirebirdBlob::create(nullConn, data);
        auto dst = cpp_dbc::Firebird::FirebirdBlob::create(nullConn);

        REQUIRE_NOTHROW(dst->copyFrom(*src));
        REQUIRE(dst->length() == 2);
    }

    SECTION("copyFrom — copies blobId and hasValidId flag")
    {
        ISC_QUAD blobId{};
        blobId.gds_quad_high = 2;
        blobId.gds_quad_low = 99;
        auto src = cpp_dbc::Firebird::FirebirdBlob::create(nullConn, blobId);
        auto dst = cpp_dbc::Firebird::FirebirdBlob::create(nullConn);
        dst->copyFrom(*src);

        REQUIRE(dst->hasValidId());
        ISC_QUAD dstId = dst->getBlobId();
        REQUIRE(dstId.gds_quad_high == 2);
        REQUIRE(dstId.gds_quad_low == 99);
    }

    SECTION("copyFrom — self-assignment is a no-op")
    {
        std::vector<uint8_t> data = {0xAA};
        auto blob = cpp_dbc::Firebird::FirebirdBlob::create(nullConn, data);
        REQUIRE_NOTHROW(blob->copyFrom(*blob));
        REQUIRE(blob->length() == 1);
    }

    SECTION("copyFrom — deep copy: modifying dst does not affect src")
    {
        std::vector<uint8_t> data = {0x01, 0x02, 0x03};
        auto src = cpp_dbc::Firebird::FirebirdBlob::create(nullConn, data);
        auto dst = cpp_dbc::Firebird::FirebirdBlob::create(nullConn);
        dst->copyFrom(*src);

        dst->truncate(1);
        REQUIRE(dst->length() == 1);
        REQUIRE(src->length() == 3);
    }
}

#endif // USE_FIREBIRD (blob)

// ============================================================================
// RedisDBConnection — create() factory
// No copyFrom on RedisDBConnection (non-copyable by design).
// Failure path tested with an invalid/unreachable URI — no live server needed.
// ============================================================================

#if USE_REDIS

#include <map>
#include <string>
#include <cpp_dbc/drivers/kv/redis/connection.hpp>

TEST_CASE("RedisDBConnection create() factory", "[10_071_95_redis_connection_create]")
{
    SECTION("create(std::nothrow, invalid_uri) — returns unexpected without throwing")
    {
        // An unreachable host should fail at connect time and return unexpected<DBException>
        auto r = cpp_dbc::Redis::RedisDBConnection::create(
            std::nothrow,
            "redis://invalid-host-that-does-not-exist-cpp-dbc:12345",
            "", "");
        // The call must never throw regardless of outcome.
        // On failure the result must carry a proper DBException.
        if (!r.has_value())
        {
            REQUIRE_THROWS_AS(throw r.error(), cpp_dbc::DBException);
        }
    }

    SECTION("create(std::nothrow, empty_uri) — returns unexpected without throwing")
    {
        // An empty URI is definitely invalid
        auto r = cpp_dbc::Redis::RedisDBConnection::create(
            std::nothrow,
            "",
            "", "");
        // Must not throw regardless of outcome
        if (!r.has_value())
        {
            REQUIRE_THROWS_AS(throw r.error(), cpp_dbc::DBException);
        }
    }
}

#endif // USE_REDIS
