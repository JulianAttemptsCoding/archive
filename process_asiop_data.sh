#!/bin/bash

# ASIoP Data Processing Automation Script
# Usage: ./process_asiop_data.sh [date]
# Example: ./process_asiop_data.sh 0122

# Check if date parameter is provided
if [ $# -eq 0 ]; then
    echo "Usage: $0 [date]"
    echo "Example: $0 0122"
    exit 1
fi

DATE=$1
BASE_DIR="$HOME/ASIoP_Data${DATE}"

# Check if base directory exists
if [ ! -d "$BASE_DIR" ]; then
    echo "Error: Directory $BASE_DIR does not exist"
    exit 1
fi

# Check if rename_csv_files.C+ exists
if [ ! -f "$BASE_DIR/rename_csv_files.C+" ]; then
    echo "Error: rename_csv_files.C+ not found in $BASE_DIR"
    exit 1
fi

echo "Starting ASIoP data processing for date: $DATE"
echo "Base directory: $BASE_DIR"

# Process each voltage level from 8kv to 14kv
for n in {8..14}; do
    WORK_DIR="$BASE_DIR/2025${DATE}-${n}kv"
    
    echo "Processing ${n}kv data..."
    echo "Working directory: $WORK_DIR"
    
    # Check if the directory exists
    if [ ! -d "$WORK_DIR" ]; then
        echo "Warning: Directory $WORK_DIR does not exist, skipping..."
        continue
    fi
    
    # Navigate to the working directory and run ROOT
    cd "$WORK_DIR" || {
        echo "Error: Cannot navigate to $WORK_DIR"
        continue
    }
    
    echo "Running ROOT script in $WORK_DIR"
    
    # Run ROOT with the rename script
    # Using root in batch mode to avoid interactive session
    root -b -q "$BASE_DIR/rename_csv_files.C+" << EOF
rename_csv_files()
.q
EOF
    
    if [ $? -eq 0 ]; then
        echo "Successfully processed ${n}kv data"
    else
        echo "Error processing ${n}kv data"
    fi
    
    echo "Completed processing for ${n}kv"
    echo "---"
done

echo "All ROOT processing completed."

# Create the consolidated folder
CONSOLIDATED_DIR="$BASE_DIR/ASIoP_Data${DATE}-all"
echo "Creating consolidated directory: $CONSOLIDATED_DIR"

if [ ! -d "$CONSOLIDATED_DIR" ]; then
    mkdir -p "$CONSOLIDATED_DIR"
    echo "Created directory: $CONSOLIDATED_DIR"
else
    echo "Directory already exists: $CONSOLIDATED_DIR"
fi

# Copy files from each renamed_csv_files folder
echo "Copying files to consolidated directory..."

for n in {8..14}; do
    SOURCE_DIR="$BASE_DIR/2025${DATE}-${n}kv/renamed_csv_files"
    
    if [ -d "$SOURCE_DIR" ]; then
        echo "Copying files from ${n}kv data..."
        
        # Count files to copy
        FILE_COUNT=$(find "$SOURCE_DIR" -type f -name "*.csv" | wc -l)
        
        if [ $FILE_COUNT -gt 0 ]; then
            # Copy all CSV files with progress indication
            find "$SOURCE_DIR" -type f -name "*.csv" -exec cp {} "$CONSOLIDATED_DIR/" \;
            echo "Copied $FILE_COUNT CSV files from ${n}kv data"
        else
            echo "No CSV files found in $SOURCE_DIR"
        fi
    else
        echo "Warning: Source directory $SOURCE_DIR does not exist"
    fi
done

# Final summary
TOTAL_FILES=$(find "$CONSOLIDATED_DIR" -type f -name "*.csv" | wc -l)
echo "---"
echo "Processing complete!"
echo "Total files in consolidated directory: $TOTAL_FILES"
echo "Consolidated directory: $CONSOLIDATED_DIR"

# Optional: List the contents of the consolidated directory
echo "---"
echo "Contents of consolidated directory:"
ls -la "$CONSOLIDATED_DIR"
