#!/bin/bash

# Function to process the log data
process_log_data() {
    local log_file="$1"
    shift
    local ops=("$@")
    local line total_count matched_count other_count
    declare -A op_counts

    # Initialize counters
    total_count=0
    other_count=0

    # Initialize the associative array for operation counts
    for op in "${ops[@]}"; do
        op_counts["$op"]=0
    done

    # Read the log file line by line
    while IFS= read -r line; do
        # Extract the operation and count
        if [[ "$line" =~ ([a-z_]+):\ count\ ([0-9]+) ]]; then
            local operation="${BASH_REMATCH[1]}"
            local count="${BASH_REMATCH[2]}"
            ((total_count += count))

            # Check if the operation is in the list of interested ops
            if [[ " ${ops[@]} " =~ " ${operation} " ]]; then
                op_counts["$operation"]=$((op_counts["$operation"] + count))
            else
                ((other_count += count))
            fi
        fi
    done < "$log_file"

    # Calculate and output the percentage for each interested operation
    for op in "${ops[@]}"; do
        local percentage
        percentage=$(awk "BEGIN {printf \"%.2f\", (${op_counts[$op]}/$total_count)*100}")
        printf "%s Operations Percentage: %s%%\n" "$op" "$percentage"
    done

    # Calculate and output the percentage for other operations
    local other_percentage
    other_percentage=$(awk "BEGIN {printf \"%.2f\", ($other_count/$total_count)*100}")
    printf "Other Operations Percentage: %s%%\n" "$other_percentage"
}

# Main function
main() {
    # Check for the correct number of arguments
    if [[ "$#" -lt 2 ]]; then
        printf "Usage: %s <log_file> <operation1> [<operation2> ...]\n" "$0" >&2
        return 1
    fi

    local log_file="$1"
    shift
    local ops=("$@")

    # Ensure the log file exists
    if [[ ! -f "$log_file" ]]; then
        printf "Error: Log file '%s' not found.\n" "$log_file" >&2
        return 1
    fi

    # Process the log data with the provided operations
    process_log_data "$log_file" "${ops[@]}"
}

# Execute the main function
main "$@"
