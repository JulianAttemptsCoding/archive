//
// File: rename_csv_files.C
//
// Description:
// This ROOT C++ macro scans the current directory for CSV files,
// creates copies of them with sequential names [0.csv, 1.csv, 2.csv, ...]
// starting from a configurable index, and places them in a separate subfolder.
//
// Dependencies:
// This script uses ROOT's TSystem for file operations.
//
// How to Run in ROOT:
// 1. Launch the ROOT interactive terminal:
//    root
//
// 2. Compile and execute this macro from the ROOT prompt:
//    .L rename_csv_files.C+
//    rename_csv_files()
//
// The '+' after the macro name tells ROOT to compile it using ACLiC.
//

#include <TSystem.h>
#include <TString.h>
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>

// --- CONFIGURATION ---
const int STARTING_INDEX = 20400;  // Change this to set starting index 'a'
const std::string SUBFOLDER_NAME = "renamed_csv_files";  // Name of the subfolder to create

// --- Function Declarations ---

// Main function to be called from ROOT
void rename_csv_files();

// Helper function to create directory if it doesn't exist
bool create_directory(const std::string& dir_path);

// Helper function to copy file from source to destination
bool copy_file(const std::string& source, const std::string& destination);

// Helper function to check if file exists
bool file_exists(const std::string& filepath);

// Helper function to check if path is a directory
bool is_directory(const std::string& path);

// --- Main Processing Function ---

void rename_csv_files() {
    std::cout << "Starting CSV file renaming and copying process..." << std::endl;
    std::cout << "Configuration:" << std::endl;
    std::cout << "  Starting index: " << STARTING_INDEX << std::endl;
    std::cout << "  Subfolder name: " << SUBFOLDER_NAME << std::endl;

    // Get the current directory
    TString current_dir = gSystem->pwd();
    std::cout << "Current directory: " << current_dir << std::endl;
    
    // Create the subfolder path
    std::string subfolder_path = std::string(current_dir.Data()) + "/" + SUBFOLDER_NAME;
    std::cout << "Subfolder path: " << subfolder_path << std::endl;

    // Create the subfolder
    if (!create_directory(subfolder_path)) {
        std::cerr << "Error: Could not create subfolder: " << subfolder_path << std::endl;
        return;
    }

    // Vector to store all CSV filenames
    std::vector<std::string> csv_files;
    
    // Open directory and collect CSV files
    void* dir_handle = gSystem->OpenDirectory(current_dir);
    if (!dir_handle) {
        std::cerr << "Error: Could not open current directory." << std::endl;
        return;
    }

    const char* entry;
    while ((entry = gSystem->GetDirEntry(dir_handle))) {
        TString filename(entry);
        // Filter for .csv files and exclude directories
        if (filename.EndsWith(".csv") && !is_directory(filename.Data())) {
            csv_files.push_back(filename.Data());
        }
    }
    gSystem->FreeDirectory(dir_handle);

    // Sort files to ensure consistent ordering
    std::sort(csv_files.begin(), csv_files.end());

    std::cout << "\nFound " << csv_files.size() << " CSV files:" << std::endl;
    for (size_t i = 0; i < csv_files.size(); i++) {
        std::cout << "  " << i << ": " << csv_files[i] << std::endl;
    }

    if (csv_files.empty()) {
        std::cout << "No CSV files found in the current directory." << std::endl;
        return;
    }

    // Copy and rename files
    std::cout << "\n=== COPYING AND RENAMING FILES ===" << std::endl;
    
    int successful_copies = 0;
    int failed_copies = 0;
    
    for (size_t i = 0; i < csv_files.size(); i++) {
        // Create source path
        std::string source_path = std::string(current_dir.Data()) + "/" + csv_files[i];
        
        // Create destination filename with sequential numbering
        int new_index = STARTING_INDEX + i;
        std::string dest_filename = std::to_string(new_index) + ".csv";
        std::string dest_path = subfolder_path + "/" + dest_filename;
        
        std::cout << "Copying: " << csv_files[i] << " -> " << dest_filename << std::endl;
        
        // Check if source file exists
        if (!file_exists(source_path)) {
            std::cerr << "  Error: Source file does not exist: " << source_path << std::endl;
            failed_copies++;
            continue;
        }
        
        // Check if destination already exists
        if (file_exists(dest_path)) {
            std::cout << "  Warning: Destination file already exists, overwriting: " << dest_path << std::endl;
        }
        
        // Copy the file
        if (copy_file(source_path, dest_path)) {
            std::cout << "  Success: " << csv_files[i] << " -> " << dest_filename << std::endl;
            successful_copies++;
        } else {
            std::cerr << "  Error: Failed to copy " << csv_files[i] << " to " << dest_filename << std::endl;
            failed_copies++;
        }
    }

    // Print summary
    std::cout << "\n=== SUMMARY ===" << std::endl;
    std::cout << "Total CSV files found: " << csv_files.size() << std::endl;
    std::cout << "Successful copies: " << successful_copies << std::endl;
    std::cout << "Failed copies: " << failed_copies << std::endl;
    std::cout << "Files renamed from index " << STARTING_INDEX << " to " << (STARTING_INDEX + csv_files.size() - 1) << std::endl;
    std::cout << "All copied files are located in: " << subfolder_path << std::endl;
    
    if (successful_copies > 0) {
        std::cout << "\n=== RENAMED FILES ===" << std::endl;
        std::cout << "Files in subfolder '" << SUBFOLDER_NAME << "':" << std::endl;
        for (size_t i = 0; i < csv_files.size(); i++) {
            int new_index = STARTING_INDEX + i;
            std::cout << "  " << new_index << ".csv (was: " << csv_files[i] << ")" << std::endl;
        }
    }
    
    std::cout << "\nProcess completed!" << std::endl;
}

// --- Helper Function Implementations ---

/**
 * @brief Creates a directory if it doesn't exist.
 * @param dir_path The path of the directory to create.
 * @return true if directory exists or was created successfully, false otherwise.
 */
bool create_directory(const std::string& dir_path) {
    // Check if directory already exists
    if (is_directory(dir_path)) {
        std::cout << "Subfolder already exists: " << dir_path << std::endl;
        return true;
    }
    
    // Try to create the directory
    int result = gSystem->mkdir(dir_path.c_str());
    if (result == 0) {
        std::cout << "Successfully created subfolder: " << dir_path << std::endl;
        return true;
    } else {
        std::cerr << "Failed to create directory: " << dir_path << " (error code: " << result << ")" << std::endl;
        return false;
    }
}

/**
 * @brief Copies a file from source to destination.
 * @param source The source file path.
 * @param destination The destination file path.
 * @return true if copy was successful, false otherwise.
 */
bool copy_file(const std::string& source, const std::string& destination) {
    // Use ROOT's TSystem to copy the file
    int result = gSystem->CopyFile(source.c_str(), destination.c_str(), kTRUE); // kTRUE = overwrite if exists
    
    if (result == 0) {
        return true;
    } else {
        std::cerr << "    Copy failed with error code: " << result << std::endl;
        return false;
    }
}

/**
 * @brief Checks if a file exists.
 * @param filepath The path to the file to check.
 * @return true if file exists, false otherwise.
 */
bool file_exists(const std::string& filepath) {
    return !gSystem->AccessPathName(filepath.c_str());
}

/**
 * @brief Checks if a path is a directory.
 * @param path The path to check.
 * @return true if path is a directory, false otherwise.
 */
bool is_directory(const std::string& path) {
    // Use ROOT's FileStat_t structure to get file information
    FileStat_t stat;
    if (gSystem->GetPathInfo(path.c_str(), stat) == 0) {
        return R_ISDIR(stat.fMode);
    }
    return false;
}