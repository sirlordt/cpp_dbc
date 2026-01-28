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
 * @file 26_081_test_scylladb_real_left_join.cpp
 * @brief Tests for ScyllaDB operations that emulate LEFT JOIN functionality
 */

#include <string>
#include <memory>
#include <vector>
#include <iostream>
#include <map>
#include <algorithm>
#include <optional>
#include <thread>
#include <chrono>

#include <catch2/catch_test_macros.hpp>

#include <cpp_dbc/cpp_dbc.hpp>
#include <cpp_dbc/core/columnar/columnar_db_connection.hpp>
#include <cpp_dbc/config/database_config.hpp>

#include "26_001_test_scylladb_real_common.hpp"

#if USE_SCYLLADB
// Test case for ScyllaDB operations that emulate LEFT JOIN
TEST_CASE("ScyllaDB LEFT JOIN emulation", "[26_081_01_scylladb_real_left_join]")
{
    // Skip these tests if we can't connect to ScyllaDB
    if (!scylla_test_helpers::canConnectToScylla())
    {
        SKIP("Cannot connect to ScyllaDB database");
        return;
    }

    // Get ScyllaDB configuration
    auto dbConfig = scylla_test_helpers::getScyllaConfig("dev_scylla");
    std::string username = dbConfig.getUsername();
    std::string password = dbConfig.getPassword();
    std::string host = dbConfig.getHost();
    int port = dbConfig.getPort();
    std::string keyspace = dbConfig.getDatabase();
    std::string connStr = "cpp_dbc:scylladb://" + host + ":" + std::to_string(port) + "/" + keyspace;

    // Register the ScyllaDB driver
    cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::ScyllaDB::ScyllaDBDriver>());

    // Get a connection
    auto conn = std::dynamic_pointer_cast<cpp_dbc::ColumnarDBConnection>(
        cpp_dbc::DriverManager::getDBConnection(connStr, username, password));
    REQUIRE(conn != nullptr);

    // Create test tables
    // In ScyllaDB, we'll create separate tables for departments and employees
    conn->executeUpdate("DROP TABLE IF EXISTS " + keyspace + ".test_departments");
    conn->executeUpdate("DROP TABLE IF EXISTS " + keyspace + ".test_employees");

    // Create test_departments table
    conn->executeUpdate(
        "CREATE TABLE " + keyspace + ".test_departments ("
                                     "department_id int PRIMARY KEY, "
                                     "name text, "
                                     "location text, "
                                     "budget double"
                                     ")");

    // Create test_employees table
    // In ScyllaDB, we can't use foreign keys, but we can store the references
    conn->executeUpdate(
        "CREATE TABLE " + keyspace + ".test_employees ("
                                     "employee_id int PRIMARY KEY, "
                                     "name text, "
                                     "department_id int, "
                                     "salary double, "
                                     "hire_date timestamp"
                                     ")");

    // Insert data into test_departments
    auto deptStmt = conn->prepareStatement(
        "INSERT INTO " + keyspace + ".test_departments (department_id, name, location, budget) "
                                    "VALUES (?, ?, ?, ?)");

    // Insert departments
    std::vector<std::tuple<int, std::string, std::string, double>> departments = {
        {1, "HR", "New York", 500000.0},
        {2, "Engineering", "San Francisco", 1000000.0},
        {3, "Marketing", "Chicago", 750000.0},
        {4, "Sales", "Los Angeles", 850000.0},
        {5, "Research", "Boston", 650000.0}};

    for (const auto &dept : departments)
    {
        deptStmt->setInt(1, std::get<0>(dept));
        deptStmt->setString(2, std::get<1>(dept));
        deptStmt->setString(3, std::get<2>(dept));
        deptStmt->setDouble(4, std::get<3>(dept));
        deptStmt->executeUpdate();
    }

    // Insert data into test_employees
    auto empStmt = conn->prepareStatement(
        "INSERT INTO " + keyspace + ".test_employees (employee_id, name, department_id, salary, hire_date) "
                                    "VALUES (?, ?, ?, ?, ?)");

    // Insert employees (some departments won't have employees)
    std::vector<std::tuple<int, std::string, int, double>> employees = {
        {101, "John Smith", 1, 65000.0},
        {102, "Jane Doe", 1, 70000.0},
        {103, "Bob Johnson", 2, 85000.0},
        {104, "Alice Brown", 2, 90000.0},
        {105, "Charlie Davis", 2, 82000.0},
        {106, "Diana Evans", 4, 75000.0},
        {107, "Edward Franklin", 4, 72000.0}};

    for (const auto &emp : employees)
    {
        empStmt->setInt(1, std::get<0>(emp));
        empStmt->setString(2, std::get<1>(emp));
        empStmt->setInt(3, std::get<2>(emp));
        empStmt->setDouble(4, std::get<3>(emp));
        empStmt->setTimestamp(5, "2023-01-15 10:00:00"); // Same hire date for simplicity
        empStmt->executeUpdate();
    }

    // Add ALLOW FILTERING clause to all queries that might need it
    conn->executeUpdate("CREATE INDEX IF NOT EXISTS ON " + keyspace + ".test_employees (department_id)");

    // Wait a bit for index to be ready and data to be consistent
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    SECTION("Basic LEFT JOIN emulation")
    {
        // In ScyllaDB, we need to emulate LEFT JOIN by performing multiple queries
        // First, get all departments using the primary key index to avoid filtering issues
        // We need to select one department at a time to avoid ALLOW FILTERING
        std::vector<std::pair<int, std::string>> departmentInfo;

        // Get each department individually using primary key queries
        for (const auto &dept : departments)
        {
            int deptId = std::get<0>(dept);
            auto stmt = conn->prepareStatement("SELECT department_id, name FROM " + keyspace + ".test_departments WHERE department_id = ? ALLOW FILTERING");
            stmt->setInt(1, deptId);
            auto rsDept = stmt->executeQuery();
            if (rsDept->next())
            {
                departmentInfo.push_back({rsDept->getInt("department_id"), rsDept->getString("name")});
            }
        }

        // Prepare a vector to store the joined results
        std::vector<std::tuple<int, std::string, std::optional<int>, std::optional<std::string>>> joinResults;

        // For each department, get the employees (if any)
        for (const auto &dept : departmentInfo)
        {
            int deptId = dept.first;
            std::string deptName = dept.second;

            // Get employees for this department
            auto localStmt = conn->prepareStatement("SELECT * FROM " + keyspace + ".test_employees WHERE department_id = ? ALLOW FILTERING");
            localStmt->setInt(1, deptId);
            auto rsEmployees = localStmt->executeQuery();

            bool hasEmployees = false;
            while (rsEmployees->next())
            {
                hasEmployees = true;
                int empId = rsEmployees->getInt("employee_id");
                std::string empName = rsEmployees->getString("name");

                // Add to results
                joinResults.push_back(std::make_tuple(deptId, deptName, std::optional<int>(empId), std::optional<std::string>(empName)));
            }

            // If no employees, add department with NULL employee
            if (!hasEmployees)
            {
                joinResults.push_back(std::make_tuple(deptId, deptName, std::nullopt, std::nullopt));
            }
        }

        // Sort results by department_id and employee_id
        std::sort(joinResults.begin(), joinResults.end(),
                  [](const auto &a, const auto &b)
                  {
                      if (std::get<0>(a) != std::get<0>(b))
                      {
                          return std::get<0>(a) < std::get<0>(b);
                      }
                      // Handle nullopt in employee_id
                      if (!std::get<2>(a).has_value() && std::get<2>(b).has_value())
                      {
                          return false;
                      }
                      if (std::get<2>(a).has_value() && !std::get<2>(b).has_value())
                      {
                          return true;
                      }
                      if (!std::get<2>(a).has_value() && !std::get<2>(b).has_value())
                      {
                          return false;
                      }
                      return std::get<2>(a).value() < std::get<2>(b).value();
                  });

        // Verify results
        // Expected: All departments with their employees, and departments with no employees should have NULL employee values
        REQUIRE(joinResults.size() == 9); // 7 employees + 2 departments with no employees

        // Check departments with employees
        // HR department (id 1) with 2 employees
        REQUIRE(std::get<0>(joinResults[0]) == 1);
        REQUIRE(std::get<1>(joinResults[0]) == "HR");
        REQUIRE(std::get<2>(joinResults[0]).has_value());
        REQUIRE(std::get<2>(joinResults[0]).value() == 101);
        REQUIRE(std::get<3>(joinResults[0]).has_value());
        REQUIRE(std::get<3>(joinResults[0]).value() == "John Smith");

        REQUIRE(std::get<0>(joinResults[1]) == 1);
        REQUIRE(std::get<1>(joinResults[1]) == "HR");
        REQUIRE(std::get<2>(joinResults[1]).has_value());
        REQUIRE(std::get<2>(joinResults[1]).value() == 102);
        REQUIRE(std::get<3>(joinResults[1]).has_value());
        REQUIRE(std::get<3>(joinResults[1]).value() == "Jane Doe");

        // Engineering department (id 2) with 3 employees
        REQUIRE(std::get<0>(joinResults[2]) == 2);
        REQUIRE(std::get<1>(joinResults[2]) == "Engineering");
        REQUIRE(std::get<2>(joinResults[2]).has_value());
        REQUIRE(std::get<3>(joinResults[2]).has_value());

        REQUIRE(std::get<0>(joinResults[3]) == 2);
        REQUIRE(std::get<1>(joinResults[3]) == "Engineering");
        REQUIRE(std::get<2>(joinResults[3]).has_value());
        REQUIRE(std::get<3>(joinResults[3]).has_value());

        REQUIRE(std::get<0>(joinResults[4]) == 2);
        REQUIRE(std::get<1>(joinResults[4]) == "Engineering");
        REQUIRE(std::get<2>(joinResults[4]).has_value());
        REQUIRE(std::get<3>(joinResults[4]).has_value());

        // Marketing department (id 3) with no employees
        REQUIRE(std::get<0>(joinResults[5]) == 3);
        REQUIRE(std::get<1>(joinResults[5]) == "Marketing");
        REQUIRE_FALSE(std::get<2>(joinResults[5]).has_value());
        REQUIRE_FALSE(std::get<3>(joinResults[5]).has_value());

        // Sales department (id 4) with 2 employees
        REQUIRE(std::get<0>(joinResults[6]) == 4);
        REQUIRE(std::get<1>(joinResults[6]) == "Sales");
        REQUIRE(std::get<2>(joinResults[6]).has_value());
        REQUIRE(std::get<3>(joinResults[6]).has_value());

        REQUIRE(std::get<0>(joinResults[7]) == 4);
        REQUIRE(std::get<1>(joinResults[7]) == "Sales");
        REQUIRE(std::get<2>(joinResults[7]).has_value());
        REQUIRE(std::get<3>(joinResults[7]).has_value());

        // Research department (id 5) with no employees
        REQUIRE(std::get<0>(joinResults[8]) == 5);
        REQUIRE(std::get<1>(joinResults[8]) == "Research");
        REQUIRE_FALSE(std::get<2>(joinResults[8]).has_value());
        REQUIRE_FALSE(std::get<3>(joinResults[8]).has_value());
    }

    SECTION("LEFT JOIN with filtering emulation")
    {
        // In ScyllaDB, we need to emulate a filtered LEFT JOIN
        // Get departments individually using primary key queries, then filter in memory
        std::vector<std::tuple<int, std::string, double>> filteredDepts;

        // Get each department individually using primary key queries and filter in memory
        for (const auto &dept : departments)
        {
            int deptId = std::get<0>(dept);
            auto stmt = conn->prepareStatement("SELECT department_id, name, budget FROM " + keyspace + ".test_departments WHERE department_id = ? ALLOW FILTERING");
            stmt->setInt(1, deptId);
            auto rsDept = stmt->executeQuery();
            if (rsDept->next())
            {
                // int departmentId = rsDept->getInt("department_id");
                std::string deptName = rsDept->getString("name");
                double budget = rsDept->getDouble("budget");

                // Apply filter: budget > 700000
                if (budget > 700000)
                {
                    filteredDepts.push_back(std::make_tuple(deptId, deptName, budget));
                }
            }
        }

        // Prepare a vector to store the joined results
        std::vector<std::tuple<int, std::string, double, std::optional<int>, std::optional<std::string>>> joinResults;

        // For each filtered department, get the employees (if any)
        for (const auto &dept : filteredDepts)
        {
            int deptId = std::get<0>(dept);
            std::string deptName = std::get<1>(dept);
            double budget = std::get<2>(dept);

            // Get employees for this department
            auto localStmt = conn->prepareStatement("SELECT * FROM " + keyspace + ".test_employees WHERE department_id = ? ALLOW FILTERING");
            localStmt->setInt(1, deptId);
            auto rsEmployees = localStmt->executeQuery();

            bool hasEmployees = false;
            while (rsEmployees->next())
            {
                hasEmployees = true;
                int empId = rsEmployees->getInt("employee_id");
                std::string empName = rsEmployees->getString("name");

                // Add to results
                joinResults.push_back(std::make_tuple(deptId, deptName, budget, std::optional<int>(empId), std::optional<std::string>(empName)));
            }

            // If no employees, add department with NULL employee
            if (!hasEmployees)
            {
                joinResults.push_back(std::make_tuple(deptId, deptName, budget, std::nullopt, std::nullopt));
            }
        }

        // Sort results by department_id and employee_id
        std::sort(joinResults.begin(), joinResults.end(),
                  [](const auto &a, const auto &b)
                  {
                      if (std::get<0>(a) != std::get<0>(b))
                      {
                          return std::get<0>(a) < std::get<0>(b);
                      }
                      // Handle nullopt in employee_id
                      if (!std::get<3>(a).has_value() && std::get<3>(b).has_value())
                      {
                          return false;
                      }
                      if (std::get<3>(a).has_value() && !std::get<3>(b).has_value())
                      {
                          return true;
                      }
                      if (!std::get<3>(a).has_value() && !std::get<3>(b).has_value())
                      {
                          return false;
                      }
                      return std::get<3>(a).value() < std::get<3>(b).value();
                  });

        // Verify results
        // Expected: Only departments with budget > 700000, with their employees (if any)
        REQUIRE(joinResults.size() == 6); // Engineering (3 employees), Marketing (0 employees), Sales (2 employees)

        // Engineering department (id 2) with 3 employees
        REQUIRE(std::get<0>(joinResults[0]) == 2);
        REQUIRE(std::get<1>(joinResults[0]) == "Engineering");
        REQUIRE(std::abs(std::get<2>(joinResults[0]) - 1000000.0) < 0.01);
        REQUIRE(std::get<3>(joinResults[0]).has_value());
        REQUIRE(std::get<4>(joinResults[0]).has_value());

        // Marketing department (id 3) with no employees
        REQUIRE(std::get<0>(joinResults[3]) == 3);
        REQUIRE(std::get<1>(joinResults[3]) == "Marketing");
        REQUIRE(std::abs(std::get<2>(joinResults[3]) - 750000.0) < 0.01);
        REQUIRE_FALSE(std::get<3>(joinResults[3]).has_value());
        REQUIRE_FALSE(std::get<4>(joinResults[3]).has_value());

        // Sales department (id 4) with 2 employees
        REQUIRE(std::get<0>(joinResults[4]) == 4);
        REQUIRE(std::get<1>(joinResults[4]) == "Sales");
        REQUIRE(std::abs(std::get<2>(joinResults[4]) - 850000.0) < 0.01);
        REQUIRE(std::get<3>(joinResults[4]).has_value());
        REQUIRE(std::get<4>(joinResults[4]).has_value());
    }

    // Clean up
    conn->executeUpdate("DROP TABLE IF EXISTS " + keyspace + ".test_employees");
    conn->executeUpdate("DROP TABLE IF EXISTS " + keyspace + ".test_departments");

    // Close the connection
    conn->close();
}
#else
TEST_CASE("ScyllaDB LEFT JOIN emulation (skipped)", "[26_081_02_scylladb_real_left_join]")
{
    SKIP("ScyllaDB support is not enabled");
}
#endif