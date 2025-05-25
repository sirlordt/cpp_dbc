# Changelog

## 2025-05-25 00:00

### Documentation Updates
* Added documentation about IntelliSense issue with USE_POSTGRESQL preprocessor definition:
  * Added to memory-bank/techContext.md under "Known VSCode Issues"
  * Added to memory-bank/activeContext.md under "Fixed VS Code Debugging Issues"
  * Added to memory-bank/progress.md under "VS Code Debugging Issues"
* Added useful Git command to memory-bank/git_commands.md:
  * Documented `git --no-pager diff --cached` for viewing staged changes
  * Added workflow for using this command when updating CHANGELOG.md
  * Added guidance for creating comprehensive commit messages

## 2025-05-24 23:07

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