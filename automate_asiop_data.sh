#!/bin/bash

# ====================
# CHANGE DATE HERE ONLY
# ====================
DATE="0718"  # Change this to your desired date (format: MMDD)

# Script to automate ASIoP_Data processing
echo "Starting ASIoP_Data automation for date: $DATE"

# Process each n from 8 to 14
for n in {8..14}; do
    echo "Processing n=$n..."
    
    # Check if directory exists
    WORK_DIR="ASIoP_Data${DATE}/2025${DATE}-${n}kv/"
    if [ ! -d "$WORK_DIR" ]; then
        echo "Error: Directory $WORK_DIR does not exist"
        continue
    fi
    
    # Navigate to the directory
    cd "$WORK_DIR" || {
        echo "Error: Could not navigate to $WORK_DIR"
        continue
    }
    
    # Check if the C file exists
    if [ ! -f "rename_csv_files.C" ]; then
        echo "Error: rename_csv_files.C not found in $(pwd)"
        cd ~
        continue
    fi
    
    echo "Running ROOT in directory: $(pwd)"
    echo "Found rename_csv_files.C: $(ls -la rename_csv_files.C)"
    
    # Create a temporary ROOT macro file to ensure proper execution
    cat > temp_run.C << 'TEMP_EOF'
{
    cout << "Loading rename_csv_files.C..." << endl;
    gROOT->LoadMacro("rename_csv_files.C+");
    cout << "Calling rename_csv_files()..." << endl;
    rename_csv_files();
    cout << "Function completed." << endl;
    gApplication->Terminate();
}
TEMP_EOF
    
    # Run root with the temporary macro
    root -l -b -q temp_run.C
    
    # Clean up temporary file
    rm -f temp_run.C
    
    # Check if the renamed_csv_files directory was created
    if [ -d "renamed_csv_files" ]; then
        echo "Success: renamed_csv_files directory created"
        echo "Files in renamed_csv_files: $(ls -la renamed_csv_files/ | wc -l) items"
    else
        echo "Warning: renamed_csv_files directory was not created"
    fi
    
    # Go back to home directory
    cd ~
    
    echo "Completed processing for n=$n"
    echo "-----------------------------------"
done

echo "Creating consolidated folder..."

# Create the "all" folder
mkdir -p "ASIoP_Data${DATE}/ASIoP_Data${DATE}-all"

# Copy all files from each renamed_csv_files folder to the consolidated folder
for n in {8..14}; do
    SOURCE_DIR="ASIoP_Data${DATE}/2025${DATE}-${n}kv/renamed_csv_files"
    DEST_DIR="ASIoP_Data${DATE}/ASIoP_Data${DATE}-all"
    
    if [ -d "$SOURCE_DIR" ]; then
        echo "Copying files from $SOURCE_DIR to $DEST_DIR"
        cp "$SOURCE_DIR"/* "$DEST_DIR"/ 2>/dev/null || echo "Warning: No files to copy from $SOURCE_DIR"
    else
        echo "Warning: Directory $SOURCE_DIR does not exist"
    fi
done

echo "Automation complete!"
echo "All files have been copied to: ASIoP_Data${DATE}/ASIoP_Data${DATE}-all"

# Optional: List the contents of the final directory
echo "Contents of final directory:"
ls -la "ASIoP_Data${DATE}/ASIoP_Data${DATE}-all"