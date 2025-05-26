#include <catch2/catch_test_macros.hpp>
#include <cpp_dbc/cpp_dbc.hpp>
#include <cpp_dbc/config/database_config.hpp>
#include <string>
#include <fstream>
#include <iostream>

// Skip tests if YAML support is not enabled
TEST_CASE("YamlConfigLoader tests (skipped)", "[config][yaml]")
{
  SKIP("YAML support is not enabled");
}