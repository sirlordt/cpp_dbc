/**
 * Copyright 2025 Tomas R Moreno P <tomasr.morenop@gmail.com>. All Rights Reserved.
 *
 * @file test_expected.cpp
 * @brief Comprehensive tests for cpp_dbc::expected implementation
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_template_test_macros.hpp>
#include <string>
#include <memory>

#include "cpp_dbc/core/db_expected.hpp"
#include "cpp_dbc/core/db_exception.hpp"

using namespace cpp_dbc;

// ============================================================================
// Test Helper Types
// ============================================================================

struct MoveOnlyType
{
    int value;

    explicit MoveOnlyType(int v) : value(v) {}
    MoveOnlyType(const MoveOnlyType &) = delete;
    MoveOnlyType &operator=(const MoveOnlyType &) = delete;
    MoveOnlyType(MoveOnlyType &&other) noexcept : value(other.value) { other.value = 0; }
    MoveOnlyType &operator=(MoveOnlyType &&other) noexcept
    {
        value = other.value;
        other.value = 0;
        return *this;
    }
};

struct SimpleError
{
    int code;
    std::string message;

    SimpleError(int c, std::string m) : code(c), message(std::move(m)) {}
};

// ============================================================================
// Basic Construction and State Tests
// ============================================================================

TEST_CASE("expected<T, E> - Basic construction with value", "[expected]")
{
    SECTION("Construct with lvalue")
    {
        int value = 42;
        expected<int, std::string> ex = value;

        REQUIRE(ex.has_value());
        REQUIRE(static_cast<bool>(ex));
        REQUIRE(*ex == 42);
    }

    SECTION("Construct with rvalue")
    {
        expected<int, std::string> ex = 42;

        REQUIRE(ex.has_value());
        REQUIRE(*ex == 42);
    }
}

TEST_CASE("expected<T, E> - Construction with error", "[expected]")
{
    SECTION("Construct with unexpected")
    {
        SimpleError err(404, "Not Found");
        auto unex = unexpected<SimpleError>(err);
        expected<int, SimpleError> ex = unex;

        REQUIRE_FALSE(ex.has_value());
        REQUIRE(ex.error().code == 404);
        REQUIRE(ex.error().message == "Not Found");
    }

    SECTION("Construct with DBException")
    {
        auto unex = unexpected<DBException>(DBException("TEST_ERROR", "Test error message"));
        expected<int, DBException> ex = unex;

        REQUIRE_FALSE(ex.has_value());
        REQUIRE(ex.error().getMark() == "TEST_ERROR");
    }
}

// ============================================================================
// Value Access Tests
// ============================================================================

TEST_CASE("expected<T, E> - Value access", "[expected]")
{
    SECTION("Access via value() - success")
    {
        expected<int, std::string> ex = 42;
        REQUIRE(ex.value() == 42);
    }

    SECTION("Access via value() - error throws")
    {
        auto unex = unexpected<std::string>(std::string("error"));
        expected<int, std::string> ex = unex;
        REQUIRE_THROWS(ex.value());
    }

    SECTION("Access via operator*")
    {
        expected<int, std::string> ex = 42;
        REQUIRE(*ex == 42);

        *ex = 100;
        REQUIRE(*ex == 100);
    }

    SECTION("Access via operator->")
    {
        expected<std::string, int> ex = std::string("Hello");
        REQUIRE(ex->size() == 5);
    }
}

TEST_CASE("expected<T, E> - Error access", "[expected]")
{
    SECTION("Access error")
    {
        auto unex = unexpected<SimpleError>(SimpleError(404, "Not Found"));
        expected<int, SimpleError> ex = unex;

        REQUIRE(ex.error().code == 404);
        REQUIRE(ex.error().message == "Not Found");
    }

    SECTION("Modify error")
    {
        auto unex = unexpected<SimpleError>(SimpleError(404, "Not Found"));
        expected<int, SimpleError> ex = unex;

        ex.error().code = 500;
        REQUIRE(ex.error().code == 500);
    }
}

// ============================================================================
// value_or Tests
// ============================================================================

TEST_CASE("expected<T, E> - value_or", "[expected]")
{
    SECTION("Has value")
    {
        expected<int, std::string> ex = 42;
        REQUIRE(ex.value_or(100) == 42);
    }

    SECTION("Has error")
    {
        auto unex = unexpected<std::string>(std::string("error"));
        expected<int, std::string> ex = unex;
        REQUIRE(ex.value_or(100) == 100);
    }
}

// ============================================================================
// Copy and Move Semantics Tests
// ============================================================================

TEST_CASE("expected<T, E> - Copy semantics", "[expected]")
{
    SECTION("Copy value")
    {
        expected<int, std::string> ex1 = 42;
        expected<int, std::string> ex2 = ex1;

        REQUIRE(ex1.has_value());
        REQUIRE(ex2.has_value());
        REQUIRE(*ex2 == 42);
    }

    SECTION("Copy error")
    {
        auto unex = unexpected<std::string>(std::string("error"));
        expected<int, std::string> ex1 = unex;
        expected<int, std::string> ex2 = ex1;

        REQUIRE_FALSE(ex1.has_value());
        REQUIRE_FALSE(ex2.has_value());
        REQUIRE(ex2.error() == "error");
    }

    SECTION("Copy assignment")
    {
        expected<int, std::string> ex1 = 42;
        expected<int, std::string> ex2 = 100;

        ex2 = ex1;
        REQUIRE(*ex2 == 42);
    }
}

TEST_CASE("expected<T, E> - Move semantics", "[expected]")
{
    SECTION("Move value")
    {
        expected<std::string, int> ex1 = std::string("Hello");
        expected<std::string, int> ex2 = std::move(ex1);

        REQUIRE(ex2.has_value());
        REQUIRE(*ex2 == "Hello");
    }

    SECTION("Move error")
    {
        auto unex = unexpected<std::string>(std::string("error"));
        expected<int, std::string> ex1 = unex;
        expected<int, std::string> ex2 = std::move(ex1);

        REQUIRE_FALSE(ex2.has_value());
        REQUIRE(ex2.error() == "error");
    }
}

TEST_CASE("expected<T, E> - Move-only types", "[expected]")
{
    SECTION("Construct with move-only type")
    {
        expected<MoveOnlyType, std::string> ex = MoveOnlyType(42);

        REQUIRE(ex.has_value());
        REQUIRE(ex->value == 42);
    }

    SECTION("Move construct")
    {
        expected<MoveOnlyType, std::string> ex1 = MoveOnlyType(42);
        expected<MoveOnlyType, std::string> ex2 = std::move(ex1);

        REQUIRE(ex2.has_value());
        REQUIRE(ex2->value == 42);
    }
}

// ============================================================================
// expected<void, E> Specialization Tests
// ============================================================================

TEST_CASE("expected<void, E> - Basic construction", "[expected][void]")
{
    SECTION("Default construction")
    {
        expected<void, std::string> ex;

        REQUIRE(ex.has_value());
        REQUIRE(static_cast<bool>(ex));
    }

    SECTION("Construction with error")
    {
        auto unex = unexpected<std::string>(std::string("error"));
        expected<void, std::string> ex = unex;

        REQUIRE_FALSE(ex.has_value());
        REQUIRE(ex.error() == "error");
    }
}

TEST_CASE("expected<void, E> - Value access", "[expected][void]")
{
    SECTION("value() on success")
    {
        expected<void, std::string> ex;
        REQUIRE_NOTHROW(ex.value());
    }

    SECTION("value() on error")
    {
        auto unex = unexpected<std::string>(std::string("error"));
        expected<void, std::string> ex = unex;
        REQUIRE_THROWS(ex.value());
    }
}

// ============================================================================
// Integration with DBException Tests
// ============================================================================

TEST_CASE("expected with DBException - Typical usage", "[expected][dbexception]")
{
    SECTION("Return value on success")
    {
        auto divide = [](int a, int b) -> expected<int, DBException>
        {
            if (b == 0)
            {
                return unexpected<DBException>(DBException("DIV_BY_ZERO", "Division by zero"));
            }
            return a / b;
        };

        auto result = divide(10, 2);
        REQUIRE(result.has_value());
        REQUIRE(*result == 5);
    }

    SECTION("Return error on failure")
    {
        auto divide = [](int a, int b) -> expected<int, DBException>
        {
            if (b == 0)
            {
                return unexpected<DBException>(DBException("DIV_BY_ZERO", "Division by zero"));
            }
            return a / b;
        };

        auto result = divide(10, 0);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error().getMark() == "DIV_BY_ZERO");
    }

    SECTION("Throw error if needed")
    {
        auto divide = [](int a, int b) -> expected<int, DBException>
        {
            if (b == 0)
            {
                return unexpected<DBException>(DBException("DIV_BY_ZERO", "Division by zero"));
            }
            return a / b;
        };

        auto result = divide(10, 0);
        if (!result)
        {
            REQUIRE_THROWS_AS(throw result.error(), DBException);
        }
    }
}

TEST_CASE("expected<void, DBException> - Typical usage", "[expected][dbexception][void]")
{
    SECTION("Success scenario")
    {
        auto validate = [](int value) -> expected<void, DBException>
        {
            if (value < 0)
            {
                return unexpected<DBException>(DBException("INVALID_VALUE", "Value must be non-negative"));
            }
            return {};
        };

        auto result = validate(42);
        REQUIRE(result.has_value());
        REQUIRE_NOTHROW(result.value());
    }

    SECTION("Error scenario")
    {
        auto validate = [](int value) -> expected<void, DBException>
        {
            if (value < 0)
            {
                return unexpected<DBException>(DBException("INVALID_VALUE", "Value must be non-negative"));
            }
            return {};
        };

        auto result = validate(-1);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error().getMark() == "INVALID_VALUE");
    }
}

// ============================================================================
// Complex Types Tests
// ============================================================================

TEST_CASE("expected - Complex types", "[expected]")
{
    SECTION("shared_ptr value")
    {
        auto ptr = std::make_shared<int>(42);
        expected<std::shared_ptr<int>, std::string> ex = ptr;

        REQUIRE(ex.has_value());
        REQUIRE(**ex == 42);
        REQUIRE(ex->use_count() == 2);
    }

    SECTION("unique_ptr value")
    {
        expected<std::unique_ptr<int>, std::string> ex = std::make_unique<int>(42);

        REQUIRE(ex.has_value());
        REQUIRE(**ex == 42);
    }

    SECTION("vector value")
    {
        std::vector<int> vec = {1, 2, 3, 4, 5};
        expected<std::vector<int>, std::string> ex = vec;

        REQUIRE(ex.has_value());
        REQUIRE(ex->size() == 5);
        REQUIRE((*ex)[2] == 3);
    }
}

// ============================================================================
// Real-world Simulation Tests
// ============================================================================

TEST_CASE("expected - Database simulation", "[expected][simulation]")
{
    struct Connection
    {
        int id;
        std::string name;
    };

    auto connect = [](const std::string &url) -> expected<Connection, DBException>
    {
        if (url.empty())
        {
            return unexpected<DBException>(DBException("EMPTY_URL", "URL cannot be empty"));
        }
        if (url == "invalid")
        {
            return unexpected<DBException>(DBException("INVALID_URL", "Invalid URL"));
        }
        return Connection{1, "test_connection"};
    };

    SECTION("Successful connection")
    {
        auto result = connect("mysql://localhost:3306");
        REQUIRE(result.has_value());
        REQUIRE(result->id == 1);
    }

    SECTION("Empty URL error")
    {
        auto result = connect("");
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error().getMark() == "EMPTY_URL");
    }

    SECTION("Invalid URL error")
    {
        auto result = connect("invalid");
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error().getMark() == "INVALID_URL");
    }
}

TEST_CASE("expected<void, E> - Transaction simulation", "[expected][void][simulation]")
{
    auto commit = [](bool should_fail) -> expected<void, DBException>
    {
        if (should_fail)
        {
            return unexpected<DBException>(DBException("COMMIT_FAILED", "Commit failed"));
        }
        return {};
    };

    SECTION("Successful commit")
    {
        auto result = commit(false);
        REQUIRE(result.has_value());
    }

    SECTION("Failed commit")
    {
        auto result = commit(true);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error().getMark() == "COMMIT_FAILED");
    }
}

// ============================================================================
// Performance Tests
// ============================================================================

TEST_CASE("expected - Size and alignment", "[expected][performance]")
{
    SECTION("Size comparison")
    {
        INFO("sizeof(expected<int, std::string>): " << sizeof(expected<int, std::string>));
        INFO("sizeof(int): " << sizeof(int));
        INFO("sizeof(std::string): " << sizeof(std::string));

        REQUIRE(sizeof(expected<int, std::string>) > sizeof(std::string));
    }

    SECTION("Size of void specialization")
    {
        INFO("sizeof(expected<void, std::string>): " << sizeof(expected<void, std::string>));

        REQUIRE(sizeof(expected<void, std::string>) <= sizeof(expected<int, std::string>));
    }
}
