//
// File: count_outlier_files.C
//
// Description:
// This ROOT C++ macro scans the current directory for CSV files (.csv),
// reads specific columns (sums columns C+D+E), calculates 3-sigma statistics
// based on ALL files combined, then counts outliers only from files in the
// range [a, a+99] where 'a' is a constant defined below.
//
// Dependencies:
// This script uses standard C++ file I/O for CSV parsing.
// No external library like libxls is required for CSV.
//
// How to Run in ROOT:
// 1. Launch the ROOT interactive terminal:
//    root
//
// 2. Compile and execute this macro from the ROOT prompt:
//    .L count_outlier_files.C+
//    count_outlier_files()
//
// The '+' after the macro name tells ROOT to compile it using ACLiC.
//

#include <TSystem.h>
#include <TString.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <fstream>   // For CSV file reading
#include <string>    // For string manipulation
#include <sstream>   // For parsing lines
#include <algorithm> // For min_element, max_element
#include <limits>    // For numeric limits

// --- CONFIGURATION ---
const int RANGE_START = 40200;  // Change this value to set the starting file index 'a'
const int RANGE_SIZE = 100; // Files from [a, a+99] = 100 files

// --- Function Declarations ---

// Main function to be called from ROOT
void count_outlier_files();

// Helper function to read data from a single CSV file
bool read_csv_data(const char* filename, std::vector<double>& time, std::vector<double>& amplitude);

// Helper function to calculate statistics (mean and standard deviation)
void calculate_stats(const std::vector<double>& data, double& mean, double& stddev);

// Helper function to count outliers beyond 3 sigma in a dataset
int count_outliers_beyond_3sigma(const std::vector<double>& data, double global_mean, double global_stddev);

// Helper function to validate numeric values
bool is_valid_number(double value);

// Helper function to trim whitespace from strings
std::string trim(const std::string& str);

// Helper function to safely parse string to double
bool safe_string_to_double(const std::string& str, double& result);

// --- Main Processing Function ---

void count_outlier_files() {
    std::cout << "Starting CSV file analysis for 3-sigma outliers..." << std::endl;
    std::cout << "Configuration: Processing files [" << RANGE_START << ", " << (RANGE_START + RANGE_SIZE - 1) << "]" << std::endl;

    // Vectors to store ALL data from ALL files for global statistics
    std::vector<double> all_amplitude_data;
    
    // Store file information
    std::vector<std::string> all_csv_files;
    std::vector<std::vector<double>> file_amplitudes; // Store amplitude data for each file
    
    int total_csv_files = 0;
    int files_processed_successfully = 0;

    // Get the current directory
    TString current_dir = gSystem->pwd();
    std::cout << "Current directory: " << current_dir << std::endl;
    
    void* dir_handle = gSystem->OpenDirectory(current_dir);

    if (!dir_handle) {
        std::cerr << "Error: Could not open current directory." << std::endl;
        return;
    }

    const char* entry;
    
    // FIRST PASS: Read all CSV files and collect all data
    std::cout << "\n=== FIRST PASS: Reading all CSV files ===" << std::endl;
    while ((entry = gSystem->GetDirEntry(dir_handle))) {
        TString filename(entry);
        // Filter for .csv files
        if (filename.EndsWith(".csv")) {
            total_csv_files++;
            std::cout << "Reading CSV file " << total_csv_files << ": " << filename << std::endl;
            
            std::vector<double> file_time, file_amplitude;
            if (read_csv_data(filename.Data(), file_time, file_amplitude)) {
                files_processed_successfully++;
                
                // Store filename and data
                all_csv_files.push_back(filename.Data());
                file_amplitudes.push_back(file_amplitude);
                
                // Add to global dataset
                all_amplitude_data.insert(all_amplitude_data.end(), file_amplitude.begin(), file_amplitude.end());
                
                std::cout << "  -> Successfully read " << file_amplitude.size() << " data points" << std::endl;
            } else {
                std::cerr << "  -> Failed to read data from " << filename << std::endl;
            }
        }
    }
    gSystem->FreeDirectory(dir_handle);

    if (all_amplitude_data.empty()) {
        std::cerr << "Error: No valid data was read from any CSV files." << std::endl;
        return;
    }

    // Calculate global statistics from ALL files
    std::cout << "\n=== CALCULATING GLOBAL STATISTICS ===" << std::endl;
    double global_mean, global_stddev;
    calculate_stats(all_amplitude_data, global_mean, global_stddev);
    
    std::cout << "Total data points from all files: " << all_amplitude_data.size() << std::endl;
    std::cout << "Global Mean: " << global_mean << std::endl;
    std::cout << "Global Standard Deviation: " << global_stddev << std::endl;
    
    if (!is_valid_number(global_mean) || !is_valid_number(global_stddev) || global_stddev == 0.0) {
        std::cerr << "Error: Invalid global statistics calculated." << std::endl;
        return;
    }

    double lower_bound = global_mean - 3 * global_stddev;
    double upper_bound = global_mean + 3 * global_stddev;
    std::cout << "Global 3-sigma bounds: [" << lower_bound << ", " << upper_bound << "]" << std::endl;

    // Check if we have enough files for the requested range
    int range_end = RANGE_START + RANGE_SIZE - 1;
    if (RANGE_START >= (int)all_csv_files.size()) {
        std::cerr << "Error: Range start (" << RANGE_START << ") is beyond available files (" << all_csv_files.size() << ")" << std::endl;
        return;
    }
    
    // Adjust range if it exceeds available files
    int actual_range_end = std::min(range_end, (int)all_csv_files.size() - 1);
    if (actual_range_end < range_end) {
        std::cout << "Warning: Requested range [" << RANGE_START << ", " << range_end << "] adjusted to [" << RANGE_START << ", " << actual_range_end << "] due to available files" << std::endl;
    }

    // SECOND PASS: Count outliers only in the specified range
    std::cout << "\n=== SECOND PASS: Counting outliers in range [" << RANGE_START << ", " << actual_range_end << "] ===" << std::endl;
    
    int total_outliers_in_range = 0;
    int files_with_outliers = 0;
    
    for (int i = RANGE_START; i <= actual_range_end; i++) {
        std::string filename = all_csv_files[i];
        std::vector<double>& amplitude_data = file_amplitudes[i];
        
        int outliers_in_file = count_outliers_beyond_3sigma(amplitude_data, global_mean, global_stddev);
        total_outliers_in_range += outliers_in_file;
        
        if (outliers_in_file > 0) {
            files_with_outliers++;
            std::cout << "File " << i << " (" << filename << "): " << outliers_in_file << " outliers" << std::endl;
        } else {
            std::cout << "File " << i << " (" << filename << "): No outliers" << std::endl;
        }
    }

    // Print final results
    std::cout << "\n=== FINAL RESULTS ===" << std::endl;
    std::cout << "Total CSV files found: " << total_csv_files << std::endl;
    std::cout << "Files successfully processed: " << files_processed_successfully << std::endl;
    std::cout << "Files analyzed in range [" << RANGE_START << ", " << actual_range_end << "]: " << (actual_range_end - RANGE_START + 1) << std::endl;
    std::cout << "Files with outliers in range: " << files_with_outliers << std::endl;
    std::cout << "Global statistics based on ALL files:" << std::endl;
    std::cout << "  Mean: " << global_mean << std::endl;
    std::cout << "  Standard Deviation: " << global_stddev << std::endl;
    std::cout << "  3-sigma bounds: [" << lower_bound << ", " << upper_bound << "]" << std::endl;

    std::cout << "\n=== ANSWER ===" << std::endl;
    std::cout << "Total number of outliers beyond 3-sigma in files [" << RANGE_START << ", " << actual_range_end << "]: " << total_outliers_in_range << std::endl;
}

// --- Helper Function Implementations ---

/**
 * @brief Reads data from a specified .csv file with comprehensive error handling.
 * @param filename The name of the CSV file.
 * @param time Vector to store time data (Column A - 0-indexed).
 * @param amplitude Vector to store summed amplitude data (Columns C+D+E - 0-indexed 2,3,4).
 * @return true if successful, false otherwise.
 */
bool read_csv_data(const char* filename, std::vector<double>& time, std::vector<double>& amplitude) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error opening CSV file: " << filename << std::endl;
        return false;
    }

    std::string line;
    int current_line_num = 0;
    const int DATA_START_ROW = 21; // Corresponds to Excel row 22 (0-indexed)
    int valid_rows = 0;
    int invalid_rows = 0;

    while (std::getline(file, line)) {
        current_line_num++;
        
        if (current_line_num <= DATA_START_ROW) {
            continue; // Skip lines until the data start row
        }

        // Skip empty lines
        if (trim(line).empty()) {
            continue;
        }

        std::stringstream ss(line);
        std::string segment;
        std::vector<double> row_values;

        // Parse each segment (column) separated by comma
        while(std::getline(ss, segment, ',')) {
            double value;
            if (safe_string_to_double(trim(segment), value)) {
                row_values.push_back(value);
            } else {
                row_values.push_back(0.0); // Use 0.0 for invalid entries
            }
        }

        // Ensure we have enough columns before accessing
        if (row_values.size() >= 5) { // Need at least 5 columns (A, B, C, D, E)
            double current_time = row_values[0]; // Column A (0-indexed)
            double current_amplitude_sum = row_values[2] + row_values[3] + row_values[4]; // Columns C, D, E

            // Validate the calculated values
            if (is_valid_number(current_time) && is_valid_number(current_amplitude_sum)) {
                time.push_back(current_time);
                amplitude.push_back(current_amplitude_sum);
                valid_rows++;
            } else {
                invalid_rows++;
            }
        } else {
            invalid_rows++;
        }
    }

    file.close();
    
    return valid_rows > 0;
}

/**
 * @brief Calculates the mean and standard deviation of a vector of doubles.
 * @param data The input data vector.
 * @param mean Output parameter for the calculated mean.
 * @param stddev Output parameter for the calculated standard deviation.
 */
void calculate_stats(const std::vector<double>& data, double& mean, double& stddev) {
    if (data.empty()) {
        mean = 0.0;
        stddev = 0.0;
        return;
    }

    // Calculate mean
    double sum = 0.0;
    for (const auto& x : data) {
        sum += x;
    }
    mean = sum / data.size();

    // Calculate standard deviation
    double sq_sum = 0.0;
    for (const auto& x : data) {
        double diff = x - mean;
        sq_sum += diff * diff;
    }
    stddev = std::sqrt(sq_sum / data.size());
}

/**
 * @brief Counts the number of outliers beyond 3 sigma in a dataset using global statistics.
 * @param data The input data vector.
 * @param global_mean The mean calculated from all files.
 * @param global_stddev The standard deviation calculated from all files.
 * @return The number of outliers found.
 */
int count_outliers_beyond_3sigma(const std::vector<double>& data, double global_mean, double global_stddev) {
    if (data.empty()) {
        return 0;
    }

    // Check if statistics are valid
    if (!is_valid_number(global_mean) || !is_valid_number(global_stddev) || global_stddev == 0.0) {
        return 0;
    }

    double lower_bound = global_mean - 3 * global_stddev;
    double upper_bound = global_mean + 3 * global_stddev;

    int outlier_count = 0;
    
    // Check each data point
    for (const auto& value : data) {
        if (value < lower_bound || value > upper_bound) {
            outlier_count++;
        }
    }

    return outlier_count;
}

/**
 * @brief Validates if a number is finite and not NaN.
 * @param value The value to check.
 * @return true if the value is valid, false otherwise.
 */
bool is_valid_number(double value) {
    return std::isfinite(value) && !std::isnan(value);
}

/**
 * @brief Trims whitespace from the beginning and end of a string.
 * @param str The string to trim.
 * @return The trimmed string.
 */
std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "";
    }
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

/**
 * @brief Safely converts a string to double with error handling.
 * @param str The string to convert.
 * @param result The output double value.
 * @return true if conversion was successful, false otherwise.
 */
bool safe_string_to_double(const std::string& str, double& result) {
    if (str.empty()) {
        return false;
    }
    
    try {
        size_t processed = 0;
        result = std::stod(str, &processed);
        
        // Check if the entire string was processed
        if (processed != str.length()) {
            return false;
        }
        
        // Check if the result is valid
        return is_valid_number(result);
        
    } catch (const std::invalid_argument&) {
        return false;
    } catch (const std::out_of_range&) {
        return false;
    }
}