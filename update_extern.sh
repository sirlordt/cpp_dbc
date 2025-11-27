#!/bin/bash

# List of files to update
FILES=(
    "libs/cpp_dbc/test/test_postgresql_real_full_join.cpp"
    "libs/cpp_dbc/test/test_postgresql_real_inner_join.cpp"
    "libs/cpp_dbc/test/test_sqlite_real_left_join.cpp"
    "libs/cpp_dbc/test/test_postgresql_real_right_join.cpp"
    "libs/cpp_dbc/test/test_mysql_real_inner_join.cpp"
    "libs/cpp_dbc/test/test_sqlite_real.cpp"
    "libs/cpp_dbc/test/test_postgresql_real_json.cpp"
    "libs/cpp_dbc/test/test_mysql_real_full_join.cpp"
    "libs/cpp_dbc/test/test_postgresql_real_left_join.cpp"
    "libs/cpp_dbc/test/test_sqlite_connection.cpp"
    "libs/cpp_dbc/test/test_mysql_real_left_join.cpp"
    "libs/cpp_dbc/test/test_sqlite_real_inner_join.cpp"
    "libs/cpp_dbc/test/test_mysql_real_right_join.cpp"
)

for file in "${FILES[@]}"; do
    echo "Updating $file..."
    
    # Replace extern declarations with a comment
    sed -i '/extern\s\+std::string\s\+getConfigFilePath();\s*/c\// Using common_test_helpers namespace for helper functions' "$file"
    
    # Replace function call without namespace to use the namespace
    sed -i 's/\bgetConfigFilePath()/common_test_helpers::getConfigFilePath()/g' "$file"
    
    # Also check for generateRandomJson if it exists
    sed -i 's/\bgenerateRandomJson(/common_test_helpers::generateRandomJson(/g' "$file"
    
    echo "Updated $file"
done

echo "All files updated"
