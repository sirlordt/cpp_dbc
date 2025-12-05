#!/bin/bash

# compare_benchmark_cpp_dbc_base_line.sh
# Script to compare two benchmark data files from cpp_dbc
# Usage: ./compare_benchmark_cpp_dbc_base_line.sh --benchmarkA=PATH_TO_FILE_A [--benchmarkB=PATH_TO_FILE_B|detect]

# Set script to exit on error
set -e

# Base directory for benchmark files
BASE_DIR="benchmark/base_line"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BENCHMARK_DIR="$SCRIPT_DIR/$BASE_DIR"

# Declare associative arrays for storing benchmark data
declare -A benchmarksA
declare -A benchmarksB
declare -A benchmark_names_A
declare -A benchmark_names_B

# Function to display usage information
function show_usage() {
    echo "Usage: $0 --benchmarkA=PATH_TO_FILE_A [--benchmarkB=PATH_TO_FILE_B|detect]"
    echo ""
    echo "Options:"
    echo "  --benchmarkA=PATH    Path to the first benchmark file or directory (required)"
    echo "                       If directory is specified with 'detect', the latest benchmark file will be used"
    echo "  --benchmarkB=PATH    Path to the second benchmark file or directory"
    echo "                       If directory is specified with 'detect', the latest benchmark file will be used"
    echo "  --filter=PATTERN     Only show tests matching the specified pattern (optional)"
    echo "                       Default is to show all tests"
    echo ""
    echo "Example:"
    echo "  $0 --benchmarkA=AMD_Ryzen_9_9950X_16_Core_Processor_16037MB_Ubuntu_22.04/benchmark-2025-12-04-15-34-32_-0800.data --benchmarkB=AMD_Ryzen_9_9950X_16_Core_Processor_16037MB_Ubuntu_22.04/detect"
    echo "  $0 --benchmarkA=AMD_Ryzen_9_9950X_16_Core_Processor_16037MB_Ubuntu_22.04/benchmark-2025-12-04-15-34-32_-0800.data --benchmarkB=AMD_Ryzen_9_9950X_16_Core_Processor_16037MB_Ubuntu_22.04/detect --filter=Batch"
    exit 1
}

# Function to find the latest benchmark file in a directory
function find_latest_benchmark() {
    local dir=$1
    
    # Find the latest benchmark file based on filename (not filesystem date)
    local latest_file=$(find "$dir" -name "benchmark-*.data" | sort -r | head -n 1)
    
    if [ -z "$latest_file" ]; then
        echo "Error: No benchmark files found in $dir" >&2
        exit 1
    fi
    
    echo "$latest_file"
}

# Function to load benchmark data into an associative array
# Usage: load_benchmark_data <file> <array_prefix>
# This populates arrays like:
#   benchmarksA["BM_MySQL_Select_Small_AllColumns_run_1"]=line
#   benchmarksA["BM_MySQL_Select_Small_AllColumns_run_2"]=line
#   benchmarksA["BM_MySQL_Select_Small_AllColumns_run_3"]=line
#   benchmarksA["BM_MySQL_Select_Small_AllColumns_mean"]=line
#   benchmarksA["BM_MySQL_Select_Small_AllColumns_median"]=line
#   benchmarksA["BM_MySQL_Select_Small_AllColumns_stddev"]=line
#   benchmarksA["BM_MySQL_Select_Small_AllColumns_cv"]=line
function load_benchmark_data() {
    local file=$1
    local array_name=$2  # "A" or "B"
    
    # Temporary associative array to track run counts per benchmark
    declare -A run_counts
    
    # Read the file line by line
    while IFS= read -r line; do
        # Skip empty lines, comments, and header lines
        [[ -z "$line" ]] && continue
        [[ "$line" =~ ^# ]] && continue
        [[ "$line" =~ ^benchmark_name ]] && continue
        [[ "$line" =~ ^database\; ]] && continue
        [[ "$line" =~ ^Time\ Command ]] && continue
        
        # Skip the "Time Command Output" section lines (database summary lines)
        # These lines start with database names like MySQL, PostgreSQL, SQLite
        [[ "$line" =~ ^MySQL\; ]] && continue
        [[ "$line" =~ ^PostgreSQL\; ]] && continue
        [[ "$line" =~ ^SQLite\; ]] && continue
        
        # Extract the benchmark name (first field)
        local bench_name=$(echo "$line" | cut -d';' -f1)
        
        # Skip if bench_name is empty
        [[ -z "$bench_name" ]] && continue
        
        # Skip lines that don't start with "BM_" (benchmark prefix)
        [[ ! "$bench_name" =~ ^BM_ ]] && continue
        
        # Determine if this is a statistics line or a run line
        if [[ "$bench_name" =~ _mean$ ]]; then
            local base_name="${bench_name%_mean}"
            local key="${base_name}_mean"
            if [[ "$array_name" == "A" ]]; then
                benchmarksA["$key"]="$line"
                benchmark_names_A["$base_name"]=1
            else
                benchmarksB["$key"]="$line"
                benchmark_names_B["$base_name"]=1
            fi
        elif [[ "$bench_name" =~ _median$ ]]; then
            local base_name="${bench_name%_median}"
            local key="${base_name}_median"
            if [[ "$array_name" == "A" ]]; then
                benchmarksA["$key"]="$line"
                benchmark_names_A["$base_name"]=1
            else
                benchmarksB["$key"]="$line"
                benchmark_names_B["$base_name"]=1
            fi
        elif [[ "$bench_name" =~ _stddev$ ]]; then
            local base_name="${bench_name%_stddev}"
            local key="${base_name}_stddev"
            if [[ "$array_name" == "A" ]]; then
                benchmarksA["$key"]="$line"
                benchmark_names_A["$base_name"]=1
            else
                benchmarksB["$key"]="$line"
                benchmark_names_B["$base_name"]=1
            fi
        elif [[ "$bench_name" =~ _cv$ ]]; then
            local base_name="${bench_name%_cv}"
            local key="${base_name}_cv"
            if [[ "$array_name" == "A" ]]; then
                benchmarksA["$key"]="$line"
                benchmark_names_A["$base_name"]=1
            else
                benchmarksB["$key"]="$line"
                benchmark_names_B["$base_name"]=1
            fi
        else
            # This is an individual run line
            local base_name="$bench_name"
            
            # Increment run count for this benchmark
            if [[ -z "${run_counts[$base_name]}" ]]; then
                run_counts["$base_name"]=1
            else
                run_counts["$base_name"]=$((run_counts["$base_name"] + 1))
            fi
            
            local run_num="${run_counts[$base_name]}"
            local key="${base_name}_run_${run_num}"
            
            if [[ "$array_name" == "A" ]]; then
                benchmarksA["$key"]="$line"
                benchmark_names_A["$base_name"]=1
            else
                benchmarksB["$key"]="$line"
                benchmark_names_B["$base_name"]=1
            fi
        fi
    done < "$file"
}

# Function to extract numeric value and unit from a time string
function extract_time_parts() {
    local time_str=$1
    local value=$(echo "$time_str" | sed -E 's/([0-9.]+)([ ][a-zA-Z]+|$)/\1/')
    local unit=$(echo "$time_str" | sed -E 's/[0-9.]+[ ]?([a-zA-Z]+|$)/\1/')
    
    # If unit is empty, default to ns (nanoseconds)
    if [ -z "$unit" ]; then
        unit="ns"
    fi
    
    echo "$value $unit"
}

# Function to convert time to nanoseconds
function convert_to_ns() {
    local value=$1
    local unit=$2
    
    case "$unit" in
        "ns")
            echo "$value"
            ;;
        "us")
            awk -v v="$value" 'BEGIN {printf "%.3f", v * 1000}'
            ;;
        "ms")
            awk -v v="$value" 'BEGIN {printf "%.3f", v * 1000000}'
            ;;
        "s")
            awk -v v="$value" 'BEGIN {printf "%.3f", v * 1000000000}'
            ;;
        *)
            # Default to original value if unit is unknown
            echo "$value"
            ;;
    esac
}

# Function to compare a single data line (run or statistic)
function compare_data_line() {
    local line_a=$1
    local line_b=$2
    local label=$3
    local is_cv=$4  # "true" or "false"
    
    # Initialize variables with default values
    local time_a="0"
    local cpu_time_a="0"
    local iterations_a="0"
    local items_per_second_a="0"
    local memory_a="0"
    
    local time_b="0"
    local cpu_time_b="0"
    local iterations_b="0"
    local items_per_second_b="0"
    local memory_b="0"
    
    echo "--- $label ---"
    
    # Extract metrics from A if available
    if [ -n "$line_a" ]; then
        time_a=$(echo "$line_a" | cut -d';' -f2)
        cpu_time_a=$(echo "$line_a" | cut -d';' -f3)
        iterations_a=$(echo "$line_a" | cut -d';' -f4)
        items_per_second_a=$(echo "$line_a" | cut -d';' -f5)
        memory_a=$(echo "$line_a" | cut -d';' -f6)
        echo "A: $time_a;$cpu_time_a;$iterations_a;$items_per_second_a;$memory_a"
    else
        echo "A: (not found in benchmark A)"
    fi
    
    echo ""
    
    # Extract metrics from B if available
    if [ -n "$line_b" ]; then
        time_b=$(echo "$line_b" | cut -d';' -f2)
        cpu_time_b=$(echo "$line_b" | cut -d';' -f3)
        iterations_b=$(echo "$line_b" | cut -d';' -f4)
        items_per_second_b=$(echo "$line_b" | cut -d';' -f5)
        memory_b=$(echo "$line_b" | cut -d';' -f6)
        echo "B: $time_b;$cpu_time_b;$iterations_b;$items_per_second_b;$memory_b"
    else
        echo "B: (not found in benchmark B)"
    fi
    
    echo ""
    
    # Calculate differences only if both have data
    if [ -n "$line_a" ] && [ -n "$line_b" ]; then
        if [ "$is_cv" == "true" ]; then
            # Special handling for CV section
            local cv_a_time=$(echo "$time_a" | grep -oE '^[0-9.]+' | head -1)
            local cv_b_time=$(echo "$time_b" | grep -oE '^[0-9.]+' | head -1)
            local cv_a_cpu=$(echo "$cpu_time_a" | grep -oE '^[0-9.]+' | head -1)
            local cv_b_cpu=$(echo "$cpu_time_b" | grep -oE '^[0-9.]+' | head -1)
            
            cv_a_time=${cv_a_time:-0}
            cv_b_time=${cv_b_time:-0}
            cv_a_cpu=${cv_a_cpu:-0}
            cv_b_cpu=${cv_b_cpu:-0}
            
            local cv_time_diff=$(awk -v a="$cv_a_time" -v b="$cv_b_time" 'BEGIN {printf "%.3f", b - a}')
            local cv_cpu_diff=$(awk -v a="$cv_a_cpu" -v b="$cv_b_cpu" 'BEGIN {printf "%.3f", b - a}')
            
            local cv_time_percent="0.00"
            local cv_cpu_percent="0.00"
            if [ "$cv_a_time" != "0" ] && [ -n "$cv_a_time" ]; then
                cv_time_percent=$(awk -v a="$cv_a_time" -v b="$cv_b_time" 'BEGIN {printf "%.2f", (b - a) / a * 100}')
            fi
            if [ "$cv_a_cpu" != "0" ] && [ -n "$cv_a_cpu" ]; then
                cv_cpu_percent=$(awk -v a="$cv_a_cpu" -v b="$cv_b_cpu" 'BEGIN {printf "%.2f", (b - a) / a * 100}')
            fi
            
            local time_diff_formatted="${cv_time_diff}% (${cv_time_percent}%)"
            local cpu_time_diff_formatted="${cv_cpu_diff}% (${cv_cpu_percent}%)"
            
            local ips_a_cv=$(echo "$items_per_second_a" | grep -oE '^[0-9.]+' | head -1)
            local ips_b_cv=$(echo "$items_per_second_b" | grep -oE '^[0-9.]+' | head -1)
            ips_a_cv=${ips_a_cv:-0}
            ips_b_cv=${ips_b_cv:-0}
            
            local ips_cv_diff=$(awk -v a="$ips_a_cv" -v b="$ips_b_cv" 'BEGIN {printf "%.3f", b - a}')
            local ips_cv_percent="0.00"
            if [ "$ips_a_cv" != "0" ] && [ -n "$ips_a_cv" ]; then
                ips_cv_percent=$(awk -v a="$ips_a_cv" -v b="$ips_b_cv" 'BEGIN {printf "%.2f", (b - a) / a * 100}')
            fi
            local ips_diff_formatted="${ips_cv_diff}% (${ips_cv_percent}%)"
            
            local iterations_diff=$(($iterations_b - $iterations_a))
            local iterations_percent=$(awk -v ib="$iterations_b" -v ia="$iterations_a" 'BEGIN {if (ia != 0) printf "%.2f", (ib - ia) / ia * 100; else print "0.00"}')
            local iterations_diff_formatted="$iterations_diff (${iterations_percent}%)"
            
            local memory_diff=$(($memory_b - $memory_a))
            local memory_percent=$(awk -v mb="$memory_b" -v ma="$memory_a" 'BEGIN {if (ma != 0) printf "%.2f", (mb - ma) / ma * 100; else print "0.00"}')
            local memory_diff_formatted="$memory_diff (${memory_percent}%)"
            
            echo "C: $time_diff_formatted;$cpu_time_diff_formatted;$iterations_diff_formatted;$ips_diff_formatted;$memory_diff_formatted"
        else
            # Normal time-based comparison
            local time_a_parts=$(extract_time_parts "$time_a")
            local time_a_value=$(echo "$time_a_parts" | cut -d' ' -f1)
            local time_a_unit=$(echo "$time_a_parts" | cut -d' ' -f2)
            
            local time_b_parts=$(extract_time_parts "$time_b")
            local time_b_value=$(echo "$time_b_parts" | cut -d' ' -f1)
            local time_b_unit=$(echo "$time_b_parts" | cut -d' ' -f2)
            
            local cpu_time_a_parts=$(extract_time_parts "$cpu_time_a")
            local cpu_time_a_value=$(echo "$cpu_time_a_parts" | cut -d' ' -f1)
            local cpu_time_a_unit=$(echo "$cpu_time_a_parts" | cut -d' ' -f2)
            
            local cpu_time_b_parts=$(extract_time_parts "$cpu_time_b")
            local cpu_time_b_value=$(echo "$cpu_time_b_parts" | cut -d' ' -f1)
            local cpu_time_b_unit=$(echo "$cpu_time_b_parts" | cut -d' ' -f2)
            
            local time_a_ns=$time_a_value
            local time_b_ns=$time_b_value
            local cpu_time_a_ns=$cpu_time_a_value
            local cpu_time_b_ns=$cpu_time_b_value
            
            if [ "$time_a_unit" != "$time_b_unit" ]; then
                echo "Warning: Time units differ between benchmarks ($time_a_unit vs $time_b_unit). Converting to common unit." >&2
                time_a_ns=$(convert_to_ns "$time_a_value" "$time_a_unit")
                time_b_ns=$(convert_to_ns "$time_b_value" "$time_b_unit")
            fi
            
            if [ "$cpu_time_a_unit" != "$cpu_time_b_unit" ]; then
                echo "Warning: CPU time units differ between benchmarks ($cpu_time_a_unit vs $cpu_time_b_unit). Converting to common unit." >&2
                cpu_time_a_ns=$(convert_to_ns "$cpu_time_a_value" "$cpu_time_a_unit")
                cpu_time_b_ns=$(convert_to_ns "$cpu_time_b_value" "$cpu_time_b_unit")
            fi
            
            local time_diff=$(awk -v tb="$time_b_ns" -v ta="$time_a_ns" 'BEGIN {printf "%.3f", tb - ta}')
            local cpu_time_diff=$(awk -v ctb="$cpu_time_b_ns" -v cta="$cpu_time_a_ns" 'BEGIN {printf "%.3f", ctb - cta}')
            local iterations_diff=$(($iterations_b - $iterations_a))
            local iterations_percent=$(awk -v ib="$iterations_b" -v ia="$iterations_a" 'BEGIN {if (ia != 0) printf "%.2f", (ib - ia) / ia * 100; else print "0.00"}')
            local iterations_diff_formatted="$iterations_diff (${iterations_percent}%)"
            
            local ips_a_value=$(echo "$items_per_second_a" | sed -E 's/([0-9.]+)([kMG]\/s|\/s)/\1/')
            local ips_b_value=$(echo "$items_per_second_b" | sed -E 's/([0-9.]+)([kMG]\/s|\/s)/\1/')
            local ips_a_unit=$(echo "$items_per_second_a" | sed -E 's/[0-9.]+([kMG]\/s|\/s)/\1/')
            local ips_b_unit=$(echo "$items_per_second_b" | sed -E 's/[0-9.]+([kMG]\/s|\/s)/\1/')
            
            if [ "$ips_a_unit" == "k/s" ]; then
                ips_a_value=$(awk -v v="$ips_a_value" 'BEGIN {printf "%.6f", v * 1000}')
            elif [ "$ips_a_unit" == "M/s" ]; then
                ips_a_value=$(awk -v v="$ips_a_value" 'BEGIN {printf "%.6f", v * 1000000}')
            elif [ "$ips_a_unit" == "G/s" ]; then
                ips_a_value=$(awk -v v="$ips_a_value" 'BEGIN {printf "%.6f", v * 1000000000}')
            fi
            
            if [ "$ips_b_unit" == "k/s" ]; then
                ips_b_value=$(awk -v v="$ips_b_value" 'BEGIN {printf "%.6f", v * 1000}')
            elif [ "$ips_b_unit" == "M/s" ]; then
                ips_b_value=$(awk -v v="$ips_b_value" 'BEGIN {printf "%.6f", v * 1000000}')
            elif [ "$ips_b_unit" == "G/s" ]; then
                ips_b_value=$(awk -v v="$ips_b_value" 'BEGIN {printf "%.6f", v * 1000000000}')
            fi
            
            local ips_diff=$(awk -v b="$ips_b_value" -v a="$ips_a_value" 'BEGIN {printf "%.3f", b - a}')
            local ips_diff_percent=$(awk -v b="$ips_b_value" -v a="$ips_a_value" 'BEGIN {if (a != 0) printf "%.2f", (b - a) / a * 100; else print "0.00"}')
            
            local ips_diff_formatted="$ips_diff (${ips_diff_percent}%)"
            
            local memory_diff=$(($memory_b - $memory_a))
            local memory_percent=$(awk -v mb="$memory_b" -v ma="$memory_a" 'BEGIN {if (ma != 0) printf "%.2f", (mb - ma) / ma * 100; else print "0.00"}')
            local memory_diff_formatted="$memory_diff (${memory_percent}%)"
            
            local time_diff_formatted="${time_diff} ns"
            local cpu_time_diff_formatted="${cpu_time_diff} ns"
            
            local time_percent=$(awk -v tb="$time_b_ns" -v ta="$time_a_ns" 'BEGIN {if (ta != 0) printf "%.2f", (tb - ta) / ta * 100; else print "0.00"}')
            local cpu_time_percent=$(awk -v ctb="$cpu_time_b_ns" -v cta="$cpu_time_a_ns" 'BEGIN {if (cta != 0) printf "%.2f", (ctb - cta) / cta * 100; else print "0.00"}')
            
            time_diff_formatted="${time_diff_formatted} (${time_percent}%)"
            cpu_time_diff_formatted="${cpu_time_diff_formatted} (${cpu_time_percent}%)"
            
            echo "C: $time_diff_formatted;$cpu_time_diff_formatted;$iterations_diff_formatted;$ips_diff_formatted;$memory_diff_formatted"
        fi
    else
        echo "C: (comparison not possible - data missing in one file)"
    fi
    
    echo ""
}

# Function to compare two benchmark results using the maps
function compare_benchmarks_from_maps() {
    local test_name=$1
    local in_a=$2  # "true" or "false"
    local in_b=$3  # "true" or "false"
    
    # Print the comparison header
    echo "Benchmark: $test_name"
    echo ""
    echo "time_ms;cpu_time_ms;iterations;items_per_second;max_memory_kb"
    echo ""
    
    # Handle case where benchmark is only in one file
    if [ "$in_a" == "false" ]; then
        echo "Warning: Test $test_name not found in benchmark A" >&2
    fi
    if [ "$in_b" == "false" ]; then
        echo "Warning: Test $test_name not found in benchmark B" >&2
    fi
    
    # Process individual runs (run_1, run_2, run_3, etc.)
    echo "--- Individual Runs ---"
    
    # Find the maximum number of runs
    local max_runs=0
    for i in 1 2 3 4 5 6 7 8 9 10; do
        local key_a="${test_name}_run_${i}"
        local key_b="${test_name}_run_${i}"
        if [[ -n "${benchmarksA[$key_a]}" ]] || [[ -n "${benchmarksB[$key_b]}" ]]; then
            max_runs=$i
        fi
    done
    
    # Process each run
    for (( i=1; i<=max_runs; i++ )); do
        local key="${test_name}_run_${i}"
        local line_a="${benchmarksA[$key]}"
        local line_b="${benchmarksB[$key]}"
        
        compare_data_line "$line_a" "$line_b" "Run $i" "false"
    done
    
    # Process statistics (mean, median, stddev, cv)
    local stats=("mean" "median" "stddev" "cv")
    local stat_labels=("Mean" "Median" "StdDev" "CV")
    
    for idx in "${!stats[@]}"; do
        local stat="${stats[$idx]}"
        local label="${stat_labels[$idx]}"
        local key="${test_name}_${stat}"
        local line_a="${benchmarksA[$key]}"
        local line_b="${benchmarksB[$key]}"
        
        # Skip if neither file has this statistic
        if [[ -z "$line_a" ]] && [[ -z "$line_b" ]]; then
            continue
        fi
        
        local is_cv="false"
        if [ "$stat" == "cv" ]; then
            is_cv="true"
        fi
        
        compare_data_line "$line_a" "$line_b" "$label" "$is_cv"
    done
    
    echo "----------------------------------------"
}

# Parse command line arguments
BENCHMARK_A=""
BENCHMARK_B=""
FILTER=""
DETECT_A=false
DETECT_B=false

for arg in "$@"; do
    case $arg in
        --benchmarkA=*)
            BENCHMARK_A="${arg#*=}"
            # Check if the path ends with /detect
            if [[ "$BENCHMARK_A" == */detect ]]; then
                DETECT_A=true
                # Remove /detect from the path
                BENCHMARK_A="${BENCHMARK_A%/detect}"
            fi
            ;;
        --benchmarkB=*)
            BENCHMARK_B="${arg#*=}"
            # Check if the path ends with /detect
            if [[ "$BENCHMARK_B" == */detect ]]; then
                DETECT_B=true
                # Remove /detect from the path
                BENCHMARK_B="${BENCHMARK_B%/detect}"
            fi
            ;;
        --filter=*)
            FILTER="${arg#*=}"
            ;;
        --help|-h)
            show_usage
            ;;
        *)
            echo "Unknown option: $arg"
            show_usage
            ;;
    esac
done

# Check if required arguments are provided
if [ -z "$BENCHMARK_A" ]; then
    echo "Error: --benchmarkA is required"
    show_usage
fi

# Resolve full paths for benchmark files
if [[ "$BENCHMARK_A" != /* ]]; then
    # If not an absolute path, assume it's relative to the benchmark directory
    BENCHMARK_A_DIR="$BENCHMARK_DIR/$(dirname "$BENCHMARK_A")"
    BENCHMARK_A_FILE="$(basename "$BENCHMARK_A")"
else
    BENCHMARK_A_DIR="$(dirname "$BENCHMARK_A")"
    BENCHMARK_A_FILE="$(basename "$BENCHMARK_A")"
fi

# Handle benchmark A file
if [ "$DETECT_A" = true ]; then
    # Find the latest benchmark file in the directory
    BENCHMARK_A_FULL=$(find_latest_benchmark "$BENCHMARK_A_DIR")
else
    # Use the specified file
    BENCHMARK_A_FULL="$BENCHMARK_A_DIR/$BENCHMARK_A_FILE"
fi

# Check if benchmark A file exists
if [ ! -f "$BENCHMARK_A_FULL" ]; then
    echo "Error: Benchmark file A not found: $BENCHMARK_A_FULL"
    exit 1
fi

# Handle benchmark B file
if [ -z "$BENCHMARK_B" ]; then
    echo "Error: --benchmarkB is required"
    show_usage
fi

if [[ "$BENCHMARK_B" != /* ]]; then
    # If not an absolute path, assume it's relative to the benchmark directory
    BENCHMARK_B_DIR="$BENCHMARK_DIR/$(dirname "$BENCHMARK_B")"
    BENCHMARK_B_FILE="$(basename "$BENCHMARK_B")"
else
    BENCHMARK_B_DIR="$(dirname "$BENCHMARK_B")"
    BENCHMARK_B_FILE="$(basename "$BENCHMARK_B")"
fi

if [ "$DETECT_B" = true ]; then
    # Find the latest benchmark file in the directory
    BENCHMARK_B_FULL=$(find_latest_benchmark "$BENCHMARK_B_DIR")
    
    # Make sure we don't use the same file for both A and B
    if [ "$BENCHMARK_B_FULL" == "$BENCHMARK_A_FULL" ]; then
        # Find the second latest file
        BENCHMARK_B_FULL=$(find "$BENCHMARK_B_DIR" -name "benchmark-*.data" | sort -r | sed -n '2p')
        
        if [ -z "$BENCHMARK_B_FULL" ]; then
            echo "Error: Could not find a different benchmark file in $BENCHMARK_B_DIR"
            exit 1
        fi
    fi
else
    # Use the specified file
    BENCHMARK_B_FULL="$BENCHMARK_B_DIR/$BENCHMARK_B_FILE"
fi

# Check if benchmark B file exists
if [ ! -f "$BENCHMARK_B_FULL" ]; then
    echo "Error: Benchmark file B not found: $BENCHMARK_B_FULL"
    exit 1
fi

# Display benchmark file information
echo "Benchmark A: "
echo "$BENCHMARK_A_FULL"
echo ""
echo "Benchmark B: "
echo "$BENCHMARK_B_FULL"
echo ""

# Load benchmark data into associative arrays
echo "Loading benchmark data..."
load_benchmark_data "$BENCHMARK_A_FULL" "A"
load_benchmark_data "$BENCHMARK_B_FULL" "B"
echo "Loaded ${#benchmark_names_A[@]} benchmarks from A"
echo "Loaded ${#benchmark_names_B[@]} benchmarks from B"
echo ""

# Get all unique benchmark names from A (using A as the base)
# Also track benchmarks only in B
declare -A all_benchmark_names

# First, add all benchmarks from A
for name in "${!benchmark_names_A[@]}"; do
    all_benchmark_names["$name"]=1
done

# Then add benchmarks only in B
for name in "${!benchmark_names_B[@]}"; do
    if [[ -z "${benchmark_names_A[$name]}" ]]; then
        all_benchmark_names["$name"]=1
    fi
done

# Get sorted list of all benchmark names
ALL_TESTS=$(printf '%s\n' "${!all_benchmark_names[@]}" | sort)

# Filter tests if a filter is specified
if [ -n "$FILTER" ]; then
    FILTERED_TESTS=()
    while IFS= read -r test; do
        if [[ "$test" == *"$FILTER"* ]]; then
            FILTERED_TESTS+=("$test")
        fi
    done <<< "$ALL_TESTS"
else
    # Use all tests if no filter is specified
    FILTERED_TESTS=()
    while IFS= read -r test; do
        FILTERED_TESTS+=("$test")
    done <<< "$ALL_TESTS"
fi

# Compare benchmarks for each test using the maps
for test in "${FILTERED_TESTS[@]}"; do
    # Determine if the test exists in A and/or B
    in_a="false"
    in_b="false"
    
    if [[ -n "${benchmark_names_A[$test]}" ]]; then
        in_a="true"
    fi
    
    if [[ -n "${benchmark_names_B[$test]}" ]]; then
        in_b="true"
    fi
    
    compare_benchmarks_from_maps "$test" "$in_a" "$in_b"
    echo ""
done

exit 0