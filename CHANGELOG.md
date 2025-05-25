# Changelog

## 2025-05-25 2:06:00 PM -0700 [f4c7947]

### Database Configuration Integration
* Added integration between database configuration and connection classes
* Created new `config_integration_example.cpp` with examples of different connection methods
* Added `createConnection()` method to `DatabaseConfig` class
* Added `createConnection()` and `createConnectionPool()` methods to `DatabaseConfigManager`
* Added new methods to `DriverManager` to work with configuration classes
* Added script to run the configuration integration example

### Connection Pool Enhancements
* Moved `ConnectionPoolConfig` from connection_pool.hpp to config/database_config.hpp
* Enhanced `ConnectionPoolConfig` with more options and better encapsulation
* Added new constructors and factory methods to `ConnectionPool`
* Added integration between connection pool and database configuration

### URL Format Change
* Changed URL format from "cppdbc:" to "cpp_dbc:" for consistency
* Updated all examples, tests, and code to use the new format

### Code Organization
* Added database_config.cpp implementation file
* Added forward declarations to improve header organization
* Fixed CMakeLists.txt to include database_config.cpp
* Updated all examples to use the new configuration classes

### Documentation Updates
* Updated memory-bank/progress.md with build.dist.sh and helper.sh improvements
* Updated memory-bank/activeContext.md with recent changes
* Enhanced README.md with detailed information about build.dist.sh and --ldd option
* Updated libs/cpp_dbc/docs/cppdbc-package.md with current project structure and features
* Updated libs/cpp_dbc/docs/cppdbc-docs-en.md with new build options and helper script information
* Updated libs/cpp_dbc/docs/cppdbc-docs-es.md with the same changes in Spanish

## 2025-05-25 12:32:00 PM -0700 [4536048]

### Testing Framework and Helper Script Enhancements
* Added test management improvements:
  * Added `--test` option to build.sh
  * Created run_test.sh in project root
  * Fixed path issues in test scripts
  * Added yaml-cpp dependency for tests
  * Added auto-detection of project build state
* Enhanced helper.sh:
  * Added support for multiple commands in single invocation
  * Added `--test`, `--run-test`, `--ldd-bin` and `--run-bin` options
  * Improved error handling and reporting
  * Added executable name detection from `.dist_build`
* Updated documentation:
  * Updated README.md with testing and helper info
  * Updated memory-bank files with recent changes
  * Updated CHANGELOG.md with previous commits
  * Standardized timestamp format in CHANGELOG.md

## 2025-05-25 11:32:00 AM -0700 [289ea75]

### YAML Configuration Support
* Added optional YAML configuration support to the library
* Created database configuration classes in `include/cpp_dbc/config/database_config.hpp`
* Implemented YAML configuration loader in `include/cpp_dbc/config/yaml_config_loader.hpp` and `src/config/yaml_config_loader.cpp`
* Added `--yaml` option to `build.sh` and `build_cpp_dbc.sh` to enable YAML support
* Modified CMakeLists.txt to conditionally include YAML-related files based on USE_CPP_YAML flag

### Examples Improvements
* Added `--examples` option to `build.sh` and `build_cpp_dbc.sh` to build examples
* Created YAML configuration example in `examples/config_example.cpp`
* Added example YAML configuration file in `examples/example_config.yml`
* Created script to run the configuration example in `examples/run_config_example.sh`
* Fixed initialization issue in `examples/transaction_manager_example.cpp`

### Build System Enhancements
* Modified `build.sh` to support `--yaml` and `--examples` options
* Updated `build_cpp_dbc.sh` to support `--yaml` and `--examples` options
* Fixed issue with Conan generators directory path in `build_cpp_dbc.sh`
* Improved error handling in build scripts

## 2025-05-25 10:28:43 AM -0700 [9561f51]

### Testing Improvements
* Added `--test` option to `build.sh` to enable building tests
* Created `run_test.sh` script in the root directory to run tests
* Modified `build_cpp_dbc.sh` to accept the `--test` option
* Changed default value of `CPP_DBC_BUILD_TESTS` to `OFF` in CMakeLists.txt
* Fixed path issues in `run_test_cpp_dbc.sh`
* Added `yaml-cpp` dependency to `libs/cpp_dbc/conanfile.txt` for tests
* Added automatic project build detection in `run_test.sh`

### Helper Script Enhancements
* Improved `helper.sh` to support multiple commands in a single invocation
* Added `--test` option to build tests
* Added `--run-test` option to run tests
* Added `--ldd` option to check executable dependencies
* Added `--run-bin` option to run the executable
* Improved error handling and reporting
* Added support for getting executable name from `.dist_build`

### Documentation Updates
* Updated README.md with information about testing and helper script
* Updated memory-bank/activeContext.md with recent changes
* Updated memory-bank/progress.md with testing improvements
* Added documentation about IntelliSense issue with USE_POSTGRESQL preprocessor definition:
  * Added to memory-bank/techContext.md under "Known VSCode Issues"
  * Added to memory-bank/activeContext.md under "Fixed VS Code Debugging Issues"
  * Added to memory-bank/progress.md under "VS Code Debugging Issues"
* Added useful Git command to memory-bank/git_commands.md:
  * Documented `git --no-pager diff --cached` for viewing staged changes
  * Added workflow for using this command when updating CHANGELOG.md
  * Added guidance for creating comprehensive commit messages

## 2025-05-25 10:21:44 AM -0700 [f250d34]

### Test Integration Improvements
* Enhanced `helper.sh` with better error handling and test support
* Updated `libs/cpp_dbc/conanfile.txt` with required test dependencies
* Refined `run_test.sh` for better test execution flow

## 2025-05-25 10:06:29 AM -0700 [3b9bd04]

### Test Framework Integration
* Modified build scripts to support test compilation
* Added `run_test.sh` in the root directory for easy test execution
* Updated CMake configuration for test integration
* Improved test build and run scripts with better error handling

## 2025-05-25 09:43:28 AM -0700 [cdf29c5]

### Initial Test Implementation
* Added test directory structure in `libs/cpp_dbc/test/`
* Created test files: test_basic.cpp, test_main.cpp, test_yaml.cpp
* Added test build script `libs/cpp_dbc/build_test_cpp_dbc.sh`
* Added test run script `libs/cpp_dbc/run_test_cpp_dbc.sh`
* Added CMake configuration for tests
* Created documentation for AddressSanitizer issues in `memory-bank/asan_issues.md`
* Updated memory-bank files with test information
* Modified build configuration to support test compilation

## 2025-05-25 12:01:46 AM -0700 [525ee54]

### Documentation Updates
* Added documentation about IntelliSense issue with USE_POSTGRESQL preprocessor definition:
  * Added to memory-bank/techContext.md under "Known VSCode Issues"
  * Added to memory-bank/activeContext.md under "Fixed VS Code Debugging Issues"
  * Added to memory-bank/progress.md under "VS Code Debugging Issues"
* Added useful Git command to memory-bank/git_commands.md:
  * Documented `git --no-pager diff --cached` for viewing staged changes
  * Added workflow for using this command when updating CHANGELOG.md
  * Added guidance for creating comprehensive commit messages

## 2025-05-24 11:15:11 PM -0700 [c5615e5]

### Build System Improvements
* Added support for fmt and nlohmann_json packages in CMakeLists.txt
* Improved build type detection and configuration
* Updated CMakeUserPresets.json to use the Debug build directory
* Added Conan generators directory to include path

### New Features
* Added JSON support with nlohmann_json library
* Added JSON object creation, manipulation, and serialization examples in main.cpp

### Testing
* Added new test files using Catch2 framework:
  * catch_main.cpp - Main entry point for Catch2 tests
  * catch_test.cpp - Tests for JSON operations
  * json_test.cpp - Additional JSON functionality tests
  * main_test.cpp - Empty file for Catch2WithMain

### Documentation Updates
* Updated README.md with correct file paths reflecting the new project structure
* Updated memory-bank/systemPatterns.md with directory references
* Updated memory-bank/techContext.md to reference C++23 instead of C++11
* Added details about VSCode configuration in techContext.md
* Updated progress.md with details about project structure reorganization

### Project Structure
* Reorganized directory structure with separate src/ and include/ directories
* Improved VSCode integration with CMakeTools