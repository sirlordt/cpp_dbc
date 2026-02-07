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
 * @file 25_521_test_mongodb_real_cursor_api.cpp
 * @brief Tests for MongoDB cursor API methods (hasNext, nextDocument)
 *
 * These tests validate the cursor convenience methods that were previously untested:
 * - hasNext() - should reliably indicate if more documents exist
 * - nextDocument() - should return next document or throw if none exist
 */

#include <string>
#include <iostream>

#include <catch2/catch_test_macros.hpp>

#include <cpp_dbc/cpp_dbc.hpp>

#include "25_001_test_mongodb_real_common.hpp"

TEST_CASE("MongoDB cursor hasNext() and nextDocument() API", "[25_521_01_mongodb_cursor_api]")
{
#if USE_MONGODB
    SECTION("Test hasNext() with nextDocument() pattern")
    {
        auto conn = mongodb_test_helpers::getMongoDBConnection();
        REQUIRE(conn != nullptr);

        // Get collection
        auto collection = conn->getCollection("test_cursor_api");
        REQUIRE(collection != nullptr);

        // Drop and recreate collection
        collection->drop();

        // Insert test documents
        std::vector<std::shared_ptr<cpp_dbc::DocumentDBData>> docs;
        for (int i = 1; i <= 5; i++)
        {
            auto doc = conn->createDocument();
            doc->setInt("id", i);
            doc->setString("name", "Document " + std::to_string(i));
            docs.push_back(doc);
        }

        auto insertResult = collection->insertMany(docs);
        REQUIRE(insertResult.insertedCount == 5);

        // Test hasNext() + nextDocument() pattern
        auto cursor = collection->find();
        REQUIRE(cursor != nullptr);

        int count = 0;
        while (cursor->hasNext())
        {
            auto doc = cursor->nextDocument();
            REQUIRE(doc != nullptr);
            REQUIRE(doc->getInt("id") == ++count);
        }
        REQUIRE(count == 5);

        // Verify hasNext() returns false after exhaustion
        REQUIRE(cursor->hasNext() == false);

        // Verify nextDocument() throws when no more documents
        bool exceptionThrown = false;
        try
        {
            cursor->nextDocument();
        }
        catch (const cpp_dbc::DBException &e)
        {
            // Expected behavior - any DBException indicates the cursor is exhausted
            exceptionThrown = true;
            std::string errorMsg = e.what_s();
            // Just verify an exception was thrown - the specific message may vary
            INFO("Caught expected exception: " + errorMsg);
        }
        REQUIRE(exceptionThrown);

        // Cleanup
        collection->drop();
    }

    SECTION("Test hasNext() returns false for empty result")
    {
        auto conn = mongodb_test_helpers::getMongoDBConnection();
        REQUIRE(conn != nullptr);

        auto collection = conn->getCollection("test_empty_cursor");
        REQUIRE(collection != nullptr);

        collection->drop();

        // Query empty collection
        auto cursor = collection->find();
        REQUIRE(cursor != nullptr);

        // hasNext() should return false immediately
        REQUIRE(cursor->hasNext() == false);

        // nextDocument() should throw
        bool exceptionThrown = false;
        try
        {
            cursor->nextDocument();
        }
        catch (const cpp_dbc::DBException &e)
        {
            exceptionThrown = true;
            INFO("Caught expected exception on empty cursor: " + std::string(e.what_s()));
        }
        REQUIRE(exceptionThrown);

        collection->drop();
    }

    SECTION("Test hasNext() multiple calls without advancing")
    {
        auto conn = mongodb_test_helpers::getMongoDBConnection();
        REQUIRE(conn != nullptr);

        auto collection = conn->getCollection("test_hasnext_idempotent");
        REQUIRE(collection != nullptr);

        collection->drop();

        // Insert 3 documents
        auto doc1 = conn->createDocument();
        doc1->setInt("id", 1);
        collection->insertOne(doc1);

        auto doc2 = conn->createDocument();
        doc2->setInt("id", 2);
        collection->insertOne(doc2);

        auto doc3 = conn->createDocument();
        doc3->setInt("id", 3);
        collection->insertOne(doc3);

        auto cursor = collection->find();
        REQUIRE(cursor != nullptr);

        // Call hasNext() multiple times without advancing - should be idempotent
        REQUIRE(cursor->hasNext() == true);
        REQUIRE(cursor->hasNext() == true);
        REQUIRE(cursor->hasNext() == true);

        // Now consume all documents
        auto firstDoc = cursor->nextDocument();
        REQUIRE(firstDoc->getInt("id") == 1);

        // hasNext() should still work correctly
        REQUIRE(cursor->hasNext() == true);
        auto secondDoc = cursor->nextDocument();
        REQUIRE(secondDoc->getInt("id") == 2);

        REQUIRE(cursor->hasNext() == true);
        auto thirdDoc = cursor->nextDocument();
        REQUIRE(thirdDoc->getInt("id") == 3);

        // Now cursor should be exhausted
        REQUIRE(cursor->hasNext() == false);
        REQUIRE(cursor->hasNext() == false); // Should remain false

        collection->drop();
    }

    SECTION("Test mixing next() and hasNext()/nextDocument()")
    {
        auto conn = mongodb_test_helpers::getMongoDBConnection();
        REQUIRE(conn != nullptr);

        auto collection = conn->getCollection("test_mixed_api");
        REQUIRE(collection != nullptr);

        collection->drop();

        // Insert documents
        for (int i = 1; i <= 5; i++)
        {
            auto doc = conn->createDocument();
            doc->setInt("id", i);
            collection->insertOne(doc);
        }

        auto cursor = collection->find();
        REQUIRE(cursor != nullptr);

        // Use next() + current() for first document
        REQUIRE(cursor->next() == true);
        auto doc1 = cursor->current();
        REQUIRE(doc1->getInt("id") == 1);

        // Switch to hasNext() + nextDocument()
        REQUIRE(cursor->hasNext() == true);
        auto doc2 = cursor->nextDocument();
        REQUIRE(doc2->getInt("id") == 2);

        // Back to next() + current()
        REQUIRE(cursor->next() == true);
        auto doc3 = cursor->current();
        REQUIRE(doc3->getInt("id") == 3);

        // Finish with hasNext() + nextDocument()
        REQUIRE(cursor->hasNext() == true);
        auto doc4 = cursor->nextDocument();
        REQUIRE(doc4->getInt("id") == 4);

        REQUIRE(cursor->hasNext() == true);
        auto doc5 = cursor->nextDocument();
        REQUIRE(doc5->getInt("id") == 5);

        REQUIRE(cursor->hasNext() == false);

        collection->drop();
    }

#else
    SKIP("MongoDB support is not enabled");
#endif
}
