#!/bin/bash

# --- Configuration ---
# Names of the C++ ROOT scripts
ANALYSIS_SCRIPT="threshold_crossing_analysis.C"
PLOT_SCRIPT="csv_plotter.cpp"

# Directory containing CSV files (current directory by default)
DATA_DIR="."

# Output directory for plots (created if it doesn't exist)
PLOT_OUTPUT_DIR="./selected_plots"
mkdir -p "$PLOT_OUTPUT_DIR"

# --- Functions ---

# Function to extract information from filename
# Expects the filename without path or extension as $1
extract_info() {
    local filename="$1"
    # Remove non-digit characters to work with digits only
    local digits_only=$(echo "$filename" | sed 's/[^0-9]//g')
    local len=${#digits_only}

    # --- Extract File Number (last 2 digits) ---
    if [ $len -ge 2 ]; then
        file_number="${digits_only: -2}" # Get last two characters
    else
        # Pad with leading zero if less than 2 digits
        file_number=$(printf "%02d" "$digits_only")
    fi

    # --- Extract kV Level (3rd digit from right) ---
    kv_level="8" # Default kV
    if [ $len -ge 3 ]; then
        local third_digit="${digits_only: -3:1}" # Get 3rd from last character
        # Map digit 0-6 to kV 8-14
        case $third_digit in
            0) kv_level="8" ;;
            1) kv_level="9" ;;
            2) kv_level="10" ;;
            3) kv_level="11" ;;
            4) kv_level="12" ;;
            5) kv_level="13" ;;
            6) kv_level="14" ;;
            *) kv_level="8" ;; # Default if digit > 6
        esac
    fi

    # --- Extract Date (5th digit from right maps to a date) ---
    date_code="7/16" # Default date
    if [ $len -ge 5 ]; then
        local fifth_digit="${digits_only: -5:1}" # Get 5th from last character
        # Map digit 0-3 to dates
        case $fifth_digit in
            0) date_code="7/16" ;;
            1) date_code="7/17" ;;
            2) date_code="7/18" ;;
            3) date_code="7/21" ;;
            *) date_code="7/16" ;; # Default if digit > 3
        esac
    fi

    # Output the extracted information (can be captured by command substitution)
    echo "$date_code" "$kv_level" "$file_number"
}


# --- Main Script Logic ---

echo "Starting analysis and plotting process..."

# Counter for files processed and plotted
processed_count=0
plotted_count=0

# Loop through all CSV files in the data directory
for csvfile in "$DATA_DIR"/*.csv; do
    # Check if the glob didn't match any files
    [[ ! -e "$csvfile" ]] && { echo "No CSV files found in $DATA_DIR"; continue; }

    # Get just the filename, not the path
    filename=$(basename "$csvfile")

    echo "---------------------------------------"
    echo "Processing file: $filename"

    # 1. Run threshold analysis
    echo "  -> Running threshold analysis..."
    # Run the analysis script. Capture both output and errors.
    analysis_output=$(root -l -q -b "$ANALYSIS_SCRIPT+(\"$csvfile\")" 2>&1)
    analysis_exit_code=$?

    # Check if the analysis ran successfully (basic check)
    if [ $analysis_exit_code -ne 0 ]; then
        echo "  -> Warning: Analysis failed for $filename (Exit code: $analysis_exit_code). Skipping."
        echo "$analysis_output" | head -n 10 # Show a snippet of the error
        continue
    fi

    # Debug: Print analysis output snippet
    # echo "Analysis output snippet:"
    # echo "$analysis_output" | grep "Threshold crosses found:" | head -n 2

    # 2. Check for threshold crosses >= 2
    # Search the output for the line indicating crosses and extract the number
    crosses_line=$(echo "$analysis_output" | grep "Threshold crosses found:")
    if [[ -n "$crosses_line" ]]; then
        # Extract the number after the colon
        crosses_count=$(echo "$crosses_line" | sed -E 's/.*Threshold crosses found: ([0-9]+).*/\1/')
        # Check if the count is a number and >= 2
        if [[ $crosses_count =~ ^[0-9]+$ ]] && [ "$crosses_count" -ge 2 ]; then
            echo "  -> Found $crosses_count threshold crosses (>= 2). Proceeding to plot."
            ((processed_count++))
        else
            echo "  -> Found $crosses_count threshold crosses (< 2). Skipping plot."
            continue
        fi
    else
        echo "  -> Warning: Could not determine threshold crosses count from analysis output. Skipping plot."
        continue
    fi

    # 3. Extract title information from filename
    # Remove .csv extension for processing
    name_without_ext="${filename%.csv}"
    echo "  -> Extracting title info from filename..."
    # Call the function and read its output into variables
    read -r extracted_date extracted_kv extracted_file_num <<< $(extract_info "$name_without_ext")

    # Format the title string
    plot_title="${extracted_kv}kV, 2025${extracted_date}, Event #${extracted_file_num}"
    echo "  -> Generated plot title: $plot_title"

    # 4. Run the plotting script with the custom title
    echo "  -> Running plot script with custom title..."
    # Pass the CSV file and the title as arguments
    # Note: We need to modify plot_csv.C to accept the title as an argument.
    # For now, we will attempt to pass it, but plot_csv.C needs modification.
    # A robust way is to modify plot_csv.C to take the title as a second argument.
    # Let's assume plot_csv.C is modified to accept two arguments: filename and title.
    # If not, you'll need to adjust the C++ code accordingly.
    plot_output=$(root -l -q -b "$PLOT_SCRIPT+(\"$csvfile\", \"$plot_title\")" 2>&1)
    plot_exit_code=$?

     if [ $plot_exit_code -ne 0 ]; then
        echo "  -> Error: Plotting failed for $filename (Exit code: $plot_exit_code)."
        echo "$plot_output" | tail -n 20 # Show a snippet of the error
        continue
    else
        echo "  -> Plotting completed successfully."
    fi


    # 5. Move generated plot files to the output directory
    echo "  -> Moving generated plots to $PLOT_OUTPUT_DIR..."
    # Move the combined PDF
    if [ -f "all_channels_combined.pdf" ]; then
        # Rename the combined PDF to include the original filename stem for clarity
        mv "all_channels_combined.pdf" "${PLOT_OUTPUT_DIR}/${name_without_ext}_combined.pdf"
        echo "    -> Moved: ${name_without_ext}_combined.pdf"
    else
        echo "    -> Warning: Expected combined PDF 'all_channels_combined.pdf' not found."
    fi

    # Move individual channel plots
    for channel in Trigger Ch2 Ch3 Ch4; do
        png_file="${channel}_plot.png"
        pdf_file="${channel}_plot.pdf"
        if [ -f "$png_file" ]; then
            mv "$png_file" "${PLOT_OUTPUT_DIR}/${name_without_ext}_${png_file}"
            echo "    -> Moved: ${name_without_ext}_${png_file}"
        fi
        if [ -f "$pdf_file" ]; then
            mv "$pdf_file" "${PLOT_OUTPUT_DIR}/${name_without_ext}_${pdf_file}"
            echo "    -> Moved: ${name_without_ext}_${pdf_file}"
        fi
    done
    ((plotted_count++))

done # End of loop through CSV files

echo "---------------------------------------"
echo "Process finished."
echo "Files analyzed (with >=2 crosses): $processed_count"
echo "Files plotted: $plotted_count"
echo "Plots saved to: $PLOT_OUTPUT_DIR"