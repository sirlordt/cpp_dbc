#!/bin/bash

# Script to update copyright headers in source files
# Usage: ./update_headers.sh --path=/path/to/files/*.cpp;/another/path/*.hpp

# Define the copyright header
COPYRIGHT_HEADER='/**
 
 * Copyright 2025 Tomas R Moreno P <tomasr.morenop@gmail.com>. All Rights Reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.

 * This file is part of the cpp_dbc project and is licensed under the GNU GPL v3.
 * See the LICENSE.md file in the project root for more information.

 @file _FILENAME_
 @brief _DESCRIPTION_

*/
'

# Function to extract description from existing header
extract_description() {
    local file="$1"
    local description=""
    
    # Check if file has a Doxygen-style header with @brief
    if grep -q "@brief" "$file"; then
        # Extract the description from the @brief line
        description=$(grep -m 1 "@brief" "$file" | sed 's/.*@brief\s*//')
        echo "$description"
        return
    fi
    
    # Check if file has a header comment block
    if grep -q "^/\*" "$file"; then
        # Find the end of the header block
        local header_end=$(grep -n "\*/" "$file" | head -1 | cut -d: -f1)
        
        if [ -n "$header_end" ]; then
            # Extract the filename line (usually the second-to-last line before */)
            local filename_line=$(head -n $((header_end-2)) "$file" | tail -n 1 | xargs)
            
            # Extract the description line (usually the last line before */)
            description=$(head -n $((header_end-1)) "$file" | tail -n 1 | xargs)
            
            # Remove "Description: " prefix if present
            description=$(echo "$description" | sed 's/^Description: //' | sed 's/^@brief //')
        fi
    fi
    
    echo "$description"
}

# Function to process a single file
process_file() {
    local file="$1"
    echo "Processing $file..."
    
    # Get the filename
    filename=$(basename "$file")
    
    # Extract description from existing header if present
    description=$(extract_description "$file")
    
    # If description is empty, use a default based on filename
    if [ -z "$description" ]; then
        case "$filename" in
            *mysql*) description="Tests for MySQL database operations" ;;
            *postgresql*) description="Tests for PostgreSQL database operations" ;;
            *sqlite*) description="Tests for SQLite database operations" ;;
            *transaction*) description="Tests for transaction management" ;;
            *yaml*) description="Tests for YAML configuration loading" ;;
            *blob*) description="Tests for BLOB operations" ;;
            *connection*) description="Tests for database connections" ;;
            *config*) description="Tests for configuration handling" ;;
            *) description="Implementation for the cpp_dbc library" ;;
        esac
    fi
    
    # Create the custom header for this file
    custom_header=$(echo "$COPYRIGHT_HEADER" | sed "s/_FILENAME_/$filename/g" | sed "s/_DESCRIPTION_/$description/g")
    
    # Create a temporary file
    temp_file=$(mktemp)
    
    # Check if the file already has a header comment block
    if grep -q "^/\*" "$file"; then
        # Find the end of the header block
        header_end=$(grep -n "\*/" "$file" | head -1 | cut -d: -f1)
        
        if [ -n "$header_end" ]; then
            # Extract content after the header block
            tail -n +$((header_end+1)) "$file" > "$temp_file"
        else
            # No end of header found, just copy the file
            cp "$file" "$temp_file"
        fi
    else
        # No header found, just copy the file
        cp "$file" "$temp_file"
    fi
    
    # Add the new header and the original content
    echo "$custom_header" > "$file"
    cat "$temp_file" >> "$file"
    
    # Remove the temporary file
    rm "$temp_file"
    
    echo "  Updated header for $file"
}

# Function to find files recursively based on pattern
find_files() {
    local path_pattern="$1"
    
    # Split path and pattern
    local dir=$(dirname "$path_pattern")
    local pattern=$(basename "$path_pattern")
    
    # Find files recursively that match the pattern
    find "$dir" -type f -name "$pattern" 2>/dev/null
}

# Default paths if none provided
DEFAULT_PATHS="libs/cpp_dbc/include/cpp_dbc/*.hpp;libs/cpp_dbc/src/*.cpp;libs/cpp_dbc/test/*.cpp;libs/cpp_dbc/test/*.hpp"

# Parse command line arguments
PATHS="$DEFAULT_PATHS"

for arg in "$@"; do
    case $arg in
        --path=*)
        PATHS="${arg#*=}"
        shift
        ;;
        *)
        # Unknown option
        echo "Unknown option: $arg"
        echo "Usage: $0 [--path=/path/to/files/*.cpp;/another/path/*.hpp]"
        exit 1
        ;;
    esac
done

# Process each path pattern
IFS=';' read -ra PATH_PATTERNS <<< "$PATHS"
FILES_PROCESSED=0

for path_pattern in "${PATH_PATTERNS[@]}"; do
    echo "Searching for files matching pattern: $path_pattern"
    
    # Find files matching the pattern
    matching_files=$(find_files "$path_pattern")
    
    # Process each file
    for file in $matching_files; do
        process_file "$file"
        ((FILES_PROCESSED++))
    done
done

echo "All files processed. Total files updated: $FILES_PROCESSED"