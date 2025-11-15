cmake_minimum_required(VERSION 3.15)
project(cpp_dbc_example VERSION 0.1.0 LANGUAGES CXX)

# Configure C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find the cpp_dbc library
find_package(cpp_dbc REQUIRED)

# Create an executable and link it with cpp_dbc
add_executable(simple_example simple_example.cpp)
target_link_libraries(simple_example PRIVATE cpp_dbc::cpp_dbc)