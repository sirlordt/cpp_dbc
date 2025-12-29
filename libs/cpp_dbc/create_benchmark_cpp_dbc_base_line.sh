#!/bin/bash

# create_base_line.sh
# Script to analyze benchmark output and create a condensed baseline for comparison

set -e

# Default values
LOG_FILE=""
OUTPUT_DIR=""

# Parse command-line arguments
while [[ $# -gt 0 ]]; do
    case "$1" in
        --log-file=*)
            LOG_FILE="${1#*=}"
            shift
            ;;
        --output-dir=*)
            OUTPUT_DIR="${1#*=}"
            shift
            ;;
        --help)
            echo "Usage: $0 [OPTIONS]"
            echo "Options:"
            echo "  --log-file=PATH       Path to the benchmark log file to analyze"
            echo "  --output-dir=PATH     Directory to store the baseline data"
            echo "  --help                Show this help message"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            echo "Use --help to see available options"
            exit 1
            ;;
    esac
done

# Check if log file is provided
if [ -z "$LOG_FILE" ]; then
    echo "Error: No log file specified. Use --log-file=PATH"
    exit 1
fi

# Check if log file exists
if [ ! -f "$LOG_FILE" ]; then
    echo "Error: Log file not found: $LOG_FILE"
    exit 1
fi

# If no output directory is specified, use default
if [ -z "$OUTPUT_DIR" ]; then
    # Get CPU info without spaces, using underscores
    CPU_INFO=$(grep "model name" /proc/cpuinfo | head -n1 | sed 's/model name.*: //' | tr ' ' '_' | tr '[:punct:]' '_')
    
    # Get RAM in MB
    RAM_MB=$(free -m | grep "Mem:" | awk '{print $2}')
    
    # Get platform info
    if [ -f /etc/os-release ]; then
        . /etc/os-release
        PLATFORM=$(echo "${NAME}_${VERSION_ID}" | tr ' ' '_')
    else
        PLATFORM="Unknown"
    fi
    
    # Create output directory path with a single directory
    OUTPUT_DIR="libs/cpp_dbc/benchmark/base_line/${CPU_INFO}_${RAM_MB}MB_${PLATFORM}"
fi

# Create output directory if it doesn't exist
mkdir -p "$OUTPUT_DIR"

# Generate output filename with timestamp
TIMESTAMP=$(date '+%Y-%m-%d-%H-%M-%S_%z')
OUTPUT_FILE="${OUTPUT_DIR}/benchmark-${TIMESTAMP}.data"

echo "Analyzing benchmark log: $LOG_FILE"
echo "Output will be saved to: $OUTPUT_FILE"

# Extract and process benchmark results
# Format: benchmark_name;time_ms;cpu_time_ms;iterations;items_per_second;max_memory_kb;benchmark_type
{
    echo "benchmark_name;time_ms;cpu_time_ms;iterations;items_per_second;max_memory_kb;benchmark_type"
    
    # Extract memory usage information from time command output
    mysql_memory=0
    postgresql_memory=0
    sqlite_memory=0
    firebird_memory=0
    mongodb_memory=0
    
    # Extract full time command output for each database type
    mysql_time_output=""
    postgresql_time_output=""
    sqlite_time_output=""
    firebird_time_output=""
    mongodb_time_output=""
    
    # Extract MySQL time command output
    mysql_section=$(grep -n "Command being timed:.*BM_MySQL" "$LOG_FILE" | head -1 | cut -d':' -f1)
    if [ -n "$mysql_section" ]; then
        # Extract the complete time command output (25 lines should be enough to capture all details)
        mysql_time_output=$(tail -n +$mysql_section "$LOG_FILE" | head -25)
        
        # Extract memory usage
        mysql_memory=$(echo "$mysql_time_output" | grep "Maximum resident set size" | awk '{print $6}')
        if [ -z "$mysql_memory" ]; then
            mysql_memory=0
        fi
    fi
    
    # Extract PostgreSQL time command output
    postgresql_section=$(grep -n "Command being timed:.*BM_PostgreSQL" "$LOG_FILE" | head -1 | cut -d':' -f1)
    if [ -n "$postgresql_section" ]; then
        # Extract the complete time command output
        postgresql_time_output=$(tail -n +$postgresql_section "$LOG_FILE" | head -25)
        
        # Extract memory usage
        postgresql_memory=$(echo "$postgresql_time_output" | grep "Maximum resident set size" | awk '{print $6}')
        if [ -z "$postgresql_memory" ]; then
            postgresql_memory=0
        fi
    fi
    
    # Extract SQLite time command output
    sqlite_section=$(grep -n "Command being timed:.*BM_SQLite" "$LOG_FILE" | head -1 | cut -d':' -f1)
    if [ -n "$sqlite_section" ]; then
        # Extract the complete time command output
        sqlite_time_output=$(tail -n +$sqlite_section "$LOG_FILE" | head -25)
        
        # Extract memory usage
        sqlite_memory=$(echo "$sqlite_time_output" | grep "Maximum resident set size" | awk '{print $6}')
        if [ -z "$sqlite_memory" ]; then
            sqlite_memory=0
        fi
    fi
    
    # Extract Firebird time command output
    firebird_section=$(grep -n "Command being timed:.*BM_Firebird" "$LOG_FILE" | head -1 | cut -d':' -f1)
    if [ -n "$firebird_section" ]; then
        # Extract the complete time command output
        firebird_time_output=$(tail -n +$firebird_section "$LOG_FILE" | head -25)
        
        # Extract memory usage
        firebird_memory=$(echo "$firebird_time_output" | grep "Maximum resident set size" | awk '{print $6}')
        if [ -z "$firebird_memory" ]; then
            firebird_memory=0
        fi
        
        # Extract MongoDB time command output
        mongodb_section=$(grep -n "Command being timed:.*BM_MongoDB" "$LOG_FILE" | head -1 | cut -d':' -f1)
        if [ -n "$mongodb_section" ]; then
            # Extract the complete time command output
            mongodb_time_output=$(tail -n +$mongodb_section "$LOG_FILE" | head -25)
            
            # Extract memory usage
            mongodb_memory=$(echo "$mongodb_time_output" | grep "Maximum resident set size" | awk '{print $6}')
            if [ -z "$mongodb_memory" ]; then
                mongodb_memory=0
            fi
        fi
    fi
    
    # Write memory usage to output file
    echo "# Memory usage (KB): MySQL=$mysql_memory, PostgreSQL=$postgresql_memory, SQLite=$sqlite_memory, Firebird=$firebird_memory, MongoDB=$mongodb_memory" >> "$OUTPUT_FILE"
    
    # Extract benchmark results from the log file
    # This pattern looks for lines with benchmark results in Google Benchmark format
    grep -E "BM_[A-Za-z0-9_]+[ ]+[0-9]+" "$LOG_FILE" |
    while read -r line; do
        # Extract benchmark name
        benchmark_name=$(echo "$line" | awk '{print $1}')
        
        # Extract all values preserving original format
        # For all benchmark lines, we need to preserve the exact format of the values
        
        # For all benchmark lines, preserve the exact format from the benchmark output
        # Extract time value exactly as it appears (e.g., "187062 ns" or "0.05 %")
        time_value=$(echo "$line" | awk '{print $2" "$3}')
        
        # Extract CPU time value exactly as it appears (e.g., "45347 ns" or "0.18 %")
        cpu_time_value=$(echo "$line" | awk '{print $4" "$5}')
        
        # Extract iterations directly
        iterations=$(echo "$line" | awk '{print $6}')
        
        # Extract items_per_second if available
        items_per_second="0"
        if [[ "$line" == *"items_per_second"* ]]; then
            items_per_second=$(echo "$line" | grep -o "items_per_second=[^ ]*" | sed 's/items_per_second=//')
        fi
        
        # Determine benchmark type and associated memory usage
        memory_kb=0
        benchmark_type=""
        if [[ "$benchmark_name" == BM_MySQL_* ]]; then
            memory_kb=$mysql_memory
            benchmark_type="MySQL"
        elif [[ "$benchmark_name" == BM_PostgreSQL_* ]]; then
            memory_kb=$postgresql_memory
            benchmark_type="PostgreSQL"
        elif [[ "$benchmark_name" == BM_SQLite_* ]]; then
            memory_kb=$sqlite_memory
            benchmark_type="SQLite"
        elif [[ "$benchmark_name" == BM_Firebird_* ]]; then
            memory_kb=$firebird_memory
            benchmark_type="Firebird"
        elif [[ "$benchmark_name" == BM_MongoDB_* ]]; then
            memory_kb=$mongodb_memory
            benchmark_type="MongoDB"
        fi
        
        # Output in semicolon-separated format - preserve exact format for all values
        echo "$benchmark_name;$time_value;$cpu_time_value;$iterations;$items_per_second;$memory_kb;$benchmark_type"
    done
} >> "$OUTPUT_FILE"

echo "Benchmark analysis complete!"
echo "Baseline data saved to: $OUTPUT_FILE"

# Print summary of the results
echo "Summary of benchmark results:"
echo "----------------------------"
echo "Total benchmarks: $(grep -c ";" "$OUTPUT_FILE" | grep -v "#" | awk '{print $1-1}')"
echo "File format: Semicolon-separated with columns (benchmark_name;time_ms;cpu_time_ms;iterations;items_per_second;max_memory_kb;benchmark_type)"
echo ""
echo "Memory usage summary:"
if [ $mysql_memory -gt 0 ]; then
    echo "  MySQL benchmarks: $mysql_memory KB"
fi
if [ $postgresql_memory -gt 0 ]; then
    echo "  PostgreSQL benchmarks: $postgresql_memory KB"
fi
if [ $sqlite_memory -gt 0 ]; then
    echo "  SQLite benchmarks: $sqlite_memory KB"
fi
if [ $firebird_memory -gt 0 ]; then
    echo "  Firebird benchmarks: $firebird_memory KB"
fi
if [ $mongodb_memory -gt 0 ]; then
    echo "  MongoDB benchmarks: $mongodb_memory KB"
fi
echo ""
echo "You can compare this baseline with future runs to track performance changes."

# Add time command output in semicolon-separated format at the end of the output file
echo "" >> "$OUTPUT_FILE"
echo "# Time Command Output (Semicolon-separated format)" >> "$OUTPUT_FILE"

# Add header for time command data
echo "database;command;user_time_seconds;system_time_seconds;cpu_percent;elapsed_time;max_resident_set_kb;page_faults_major;page_faults_minor;voluntary_ctx_switches;involuntary_ctx_switches;fs_inputs;fs_outputs;exit_status" >> "$OUTPUT_FILE"

# Process MySQL time command output
if [ -n "$mysql_time_output" ]; then
    # Extract command and trim whitespace while keeping quotes
    command=$(echo "$mysql_time_output" | grep "Command being timed:" | sed 's/Command being timed: "//' | sed 's/"$//' | sed 's/^[ \t]*//;s/[ \t]*$//')
    # Extract user time
    user_time=$(echo "$mysql_time_output" | grep "User time" | awk '{print $4}')
    # Extract system time
    system_time=$(echo "$mysql_time_output" | grep "System time" | awk '{print $4}')
    # Extract CPU percent
    cpu_percent=$(echo "$mysql_time_output" | grep "Percent of CPU" | sed 's/.*: //' | sed 's/%$//')
    # Extract elapsed time
    elapsed_time=$(echo "$mysql_time_output" | grep "Elapsed" | sed 's/.*: //')
    # Extract max resident set size
    max_resident=$(echo "$mysql_time_output" | grep "Maximum resident set size" | awk '{print $6}')
    # Extract page faults
    page_faults_major=$(echo "$mysql_time_output" | grep "Major" | awk '{print $6}')
    page_faults_minor=$(echo "$mysql_time_output" | grep "Minor" | awk '{print $6}')
    # Extract context switches
    vol_ctx_switches=$(echo "$mysql_time_output" | grep "Voluntary context switches" | awk '{print $4}')
    invol_ctx_switches=$(echo "$mysql_time_output" | grep "Involuntary context switches" | awk '{print $4}')
    # Extract file system I/O
    fs_inputs=$(echo "$mysql_time_output" | grep "File system inputs" | awk '{print $4}')
    fs_outputs=$(echo "$mysql_time_output" | grep "File system outputs" | awk '{print $4}')
    # Extract exit status
    exit_status=$(echo "$mysql_time_output" | grep "Exit status" | awk '{print $3}')
    
    # Write to output file
    echo "MySQL;\"$command\";$user_time;$system_time;$cpu_percent;\"$elapsed_time\";$max_resident;$page_faults_major;$page_faults_minor;$vol_ctx_switches;$invol_ctx_switches;$fs_inputs;$fs_outputs;$exit_status" >> "$OUTPUT_FILE"
fi

# Process PostgreSQL time command output
if [ -n "$postgresql_time_output" ]; then
    # Extract command and trim whitespace while keeping quotes
    command=$(echo "$postgresql_time_output" | grep "Command being timed:" | sed 's/Command being timed: "//' | sed 's/"$//' | sed 's/^[ \t]*//;s/[ \t]*$//')
    # Extract user time
    user_time=$(echo "$postgresql_time_output" | grep "User time" | awk '{print $4}')
    # Extract system time
    system_time=$(echo "$postgresql_time_output" | grep "System time" | awk '{print $4}')
    # Extract CPU percent
    cpu_percent=$(echo "$postgresql_time_output" | grep "Percent of CPU" | sed 's/.*: //' | sed 's/%$//')
    # Extract elapsed time
    elapsed_time=$(echo "$postgresql_time_output" | grep "Elapsed" | sed 's/.*: //')
    # Extract max resident set size
    max_resident=$(echo "$postgresql_time_output" | grep "Maximum resident set size" | awk '{print $6}')
    # Extract page faults
    page_faults_major=$(echo "$postgresql_time_output" | grep "Major" | awk '{print $6}')
    page_faults_minor=$(echo "$postgresql_time_output" | grep "Minor" | awk '{print $6}')
    # Extract context switches
    vol_ctx_switches=$(echo "$postgresql_time_output" | grep "Voluntary context switches" | awk '{print $4}')
    invol_ctx_switches=$(echo "$postgresql_time_output" | grep "Involuntary context switches" | awk '{print $4}')
    # Extract file system I/O
    fs_inputs=$(echo "$postgresql_time_output" | grep "File system inputs" | awk '{print $4}')
    fs_outputs=$(echo "$postgresql_time_output" | grep "File system outputs" | awk '{print $4}')
    # Extract exit status
    exit_status=$(echo "$postgresql_time_output" | grep "Exit status" | awk '{print $3}')
    
    # Write to output file
    echo "PostgreSQL;\"$command\";$user_time;$system_time;$cpu_percent;\"$elapsed_time\";$max_resident;$page_faults_major;$page_faults_minor;$vol_ctx_switches;$invol_ctx_switches;$fs_inputs;$fs_outputs;$exit_status" >> "$OUTPUT_FILE"
fi

# Process SQLite time command output
if [ -n "$sqlite_time_output" ]; then
    # Extract command and trim whitespace while keeping quotes
    command=$(echo "$sqlite_time_output" | grep "Command being timed:" | sed 's/Command being timed: "//' | sed 's/"$//' | sed 's/^[ \t]*//;s/[ \t]*$//')
    # Extract user time
    user_time=$(echo "$sqlite_time_output" | grep "User time" | awk '{print $4}')
    # Extract system time
    system_time=$(echo "$sqlite_time_output" | grep "System time" | awk '{print $4}')
    # Extract CPU percent
    cpu_percent=$(echo "$sqlite_time_output" | grep "Percent of CPU" | sed 's/.*: //' | sed 's/%$//')
    # Extract elapsed time
    elapsed_time=$(echo "$sqlite_time_output" | grep "Elapsed" | sed 's/.*: //')
    # Extract max resident set size
    max_resident=$(echo "$sqlite_time_output" | grep "Maximum resident set size" | awk '{print $6}')
    # Extract page faults
    page_faults_major=$(echo "$sqlite_time_output" | grep "Major" | awk '{print $6}')
    page_faults_minor=$(echo "$sqlite_time_output" | grep "Minor" | awk '{print $6}')
    # Extract context switches
    vol_ctx_switches=$(echo "$sqlite_time_output" | grep "Voluntary context switches" | awk '{print $4}')
    invol_ctx_switches=$(echo "$sqlite_time_output" | grep "Involuntary context switches" | awk '{print $4}')
    # Extract file system I/O
    fs_inputs=$(echo "$sqlite_time_output" | grep "File system inputs" | awk '{print $4}')
    fs_outputs=$(echo "$sqlite_time_output" | grep "File system outputs" | awk '{print $4}')
    # Extract exit status
    exit_status=$(echo "$sqlite_time_output" | grep "Exit status" | awk '{print $3}')
    
    # Write to output file
    echo "SQLite;\"$command\";$user_time;$system_time;$cpu_percent;\"$elapsed_time\";$max_resident;$page_faults_major;$page_faults_minor;$vol_ctx_switches;$invol_ctx_switches;$fs_inputs;$fs_outputs;$exit_status" >> "$OUTPUT_FILE"
fi

# Process Firebird time command output
if [ -n "$firebird_time_output" ]; then
    # Extract command and trim whitespace while keeping quotes
    command=$(echo "$firebird_time_output" | grep "Command being timed:" | sed 's/Command being timed: "//' | sed 's/"$//' | sed 's/^[ \t]*//;s/[ \t]*$//')
    # Extract user time
    user_time=$(echo "$firebird_time_output" | grep "User time" | awk '{print $4}')
    # Extract system time
    system_time=$(echo "$firebird_time_output" | grep "System time" | awk '{print $4}')
    # Extract CPU percent
    cpu_percent=$(echo "$firebird_time_output" | grep "Percent of CPU" | sed 's/.*: //' | sed 's/%$//')
    # Extract elapsed time
    elapsed_time=$(echo "$firebird_time_output" | grep "Elapsed" | sed 's/.*: //')
    # Extract max resident set size
    max_resident=$(echo "$firebird_time_output" | grep "Maximum resident set size" | awk '{print $6}')
    # Extract page faults
    page_faults_major=$(echo "$firebird_time_output" | grep "Major" | awk '{print $6}')
    page_faults_minor=$(echo "$firebird_time_output" | grep "Minor" | awk '{print $6}')
    # Extract context switches
    vol_ctx_switches=$(echo "$firebird_time_output" | grep "Voluntary context switches" | awk '{print $4}')
    invol_ctx_switches=$(echo "$firebird_time_output" | grep "Involuntary context switches" | awk '{print $4}')
    # Extract file system I/O
    fs_inputs=$(echo "$firebird_time_output" | grep "File system inputs" | awk '{print $4}')
    fs_outputs=$(echo "$firebird_time_output" | grep "File system outputs" | awk '{print $4}')
    # Extract exit status
    exit_status=$(echo "$firebird_time_output" | grep "Exit status" | awk '{print $3}')
    
    # Write to output file
    echo "Firebird;\"$command\";$user_time;$system_time;$cpu_percent;\"$elapsed_time\";$max_resident;$page_faults_major;$page_faults_minor;$vol_ctx_switches;$invol_ctx_switches;$fs_inputs;$fs_outputs;$exit_status" >> "$OUTPUT_FILE"
fi

# Process MongoDB time command output
if [ -n "$mongodb_time_output" ]; then
    # Extract command and trim whitespace while keeping quotes
    command=$(echo "$mongodb_time_output" | grep "Command being timed:" | sed 's/Command being timed: "//' | sed 's/"$//' | sed 's/^[ \t]*//;s/[ \t]*$//')
    # Extract user time
    user_time=$(echo "$mongodb_time_output" | grep "User time" | awk '{print $4}')
    # Extract system time
    system_time=$(echo "$mongodb_time_output" | grep "System time" | awk '{print $4}')
    # Extract CPU percent
    cpu_percent=$(echo "$mongodb_time_output" | grep "Percent of CPU" | sed 's/.*: //' | sed 's/%$//')
    # Extract elapsed time
    elapsed_time=$(echo "$mongodb_time_output" | grep "Elapsed" | sed 's/.*: //')
    # Extract max resident set size
    max_resident=$(echo "$mongodb_time_output" | grep "Maximum resident set size" | awk '{print $6}')
    # Extract page faults
    page_faults_major=$(echo "$mongodb_time_output" | grep "Major" | awk '{print $6}')
    page_faults_minor=$(echo "$mongodb_time_output" | grep "Minor" | awk '{print $6}')
    # Extract context switches
    vol_ctx_switches=$(echo "$mongodb_time_output" | grep "Voluntary context switches" | awk '{print $4}')
    invol_ctx_switches=$(echo "$mongodb_time_output" | grep "Involuntary context switches" | awk '{print $4}')
    # Extract file system I/O
    fs_inputs=$(echo "$mongodb_time_output" | grep "File system inputs" | awk '{print $4}')
    fs_outputs=$(echo "$mongodb_time_output" | grep "File system outputs" | awk '{print $4}')
    # Extract exit status
    exit_status=$(echo "$mongodb_time_output" | grep "Exit status" | awk '{print $3}')
    
    # Write to output file
    echo "MongoDB;\"$command\";$user_time;$system_time;$cpu_percent;\"$elapsed_time\";$max_resident;$page_faults_major;$page_faults_minor;$vol_ctx_switches;$invol_ctx_switches;$fs_inputs;$fs_outputs;$exit_status" >> "$OUTPUT_FILE"
fi

echo "Baseline creation completed."