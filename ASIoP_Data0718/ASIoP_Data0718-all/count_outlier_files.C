//
// File: count_outlier_files.C
//
// Description:
// This ROOT C++ macro scans the current directory for CSV files (.csv),
// reads specific columns (sums columns C+D+E), calculates n-sigma statistics
// based on ALL files combined, then counts outliers from files with
// numeric names in ranges [a, a+RANGE_SIZE-1], [a+RANGE_SIZE, a+2*RANGE_SIZE-1], etc.
// The analysis is repeated for sigma thresholds from 1 to 5.
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
#include <map>       // For storing filename to index mapping

// --- CONFIGURATION ---
const int RANGE_START = 20000;   // Starting file index 'a'
const int RANGE_SIZE = 100;  // Size of each range
const int MIN_SIGMA_THRESHOLD = 1; // Minimum sigma threshold
const int MAX_SIGMA_THRESHOLD = 5; // Maximum sigma threshold

// --- Function Declarations ---

// Structure to store analysis results
struct AnalysisResult {
    int sigma_threshold;
    int range_start;
    int range_end;
    int files_analyzed;
    int files_with_outliers;
    int total_outliers;
};

// Main function to be called from ROOT
void count_outlier_files();

// Helper function to process a single range with a given sigma threshold
AnalysisResult process_range(int range_start, int current_sigma_threshold, 
                            const std::map<int, std::string>& numeric_files,
                            const std::map<int, std::vector<double>>& file_amplitudes,
                            double global_mean, double global_stddev);

// Helper function to print comprehensive final statistics
void print_final_statistics(const std::vector<AnalysisResult>& results, 
                           double global_mean, double global_stddev,
                           int total_csv_files, int files_processed_successfully,
                           int numeric_files_count);

// Helper function to read data from a single CSV file
bool read_csv_data(const char* filename, std::vector<double>& time, std::vector<double>& amplitude);

// Helper function to calculate statistics (mean and standard deviation)
void calculate_stats(const std::vector<double>& data, double& mean, double& stddev);

// Helper function to count outliers beyond n sigma in a dataset
int count_outliers_beyond_nsigma(const std::vector<double>& data, double global_mean, double global_stddev, int sigma_threshold);

// Helper function to validate numeric values
bool is_valid_number(double value);

// Helper function to trim whitespace from strings
std::string trim(const std::string& str);

// Helper function to safely parse string to double
bool safe_string_to_double(const std::string& str, double& result);

// Helper function to extract numeric index from filename
int extract_numeric_index(const std::string& filename);

// --- Main Processing Function ---

void count_outlier_files() {
    std::cout << "Starting CSV file analysis for multi-range, multi-sigma outliers..." << std::endl;
    std::cout << "Configuration: Processing ranges of size " << RANGE_SIZE << " starting from " << RANGE_START << std::endl;
    std::cout << "Sigma thresholds: " << MIN_SIGMA_THRESHOLD << " to " << MAX_SIGMA_THRESHOLD << std::endl;

    // Vectors to store ALL data from ALL files for global statistics
    std::vector<double> all_amplitude_data;
    
    // Map to store filename to data mapping
    std::map<int, std::string> numeric_files; // index -> filename
    std::map<int, std::vector<double>> file_amplitudes; // index -> amplitude data
    
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
            int file_index = extract_numeric_index(filename.Data());
            
            std::cout << "Reading CSV file: " << filename;
            if (file_index >= 0) {
                std::cout << " (index: " << file_index << ")";
            } else {
                std::cout << " (non-numeric name)";
            }
            std::cout << std::endl;
            
            std::vector<double> file_time, file_amplitude;
            if (read_csv_data(filename.Data(), file_time, file_amplitude)) {
                files_processed_successfully++;
                
                // Store data for files with numeric names
                if (file_index >= 0) {
                    numeric_files[file_index] = filename.Data();
                    file_amplitudes[file_index] = file_amplitude;
                }
                
                // Add to global dataset (ALL files contribute to global statistics)
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

    // Find the maximum file index to determine how many ranges to process
    int max_file_index = -1;
    for (const auto& pair : numeric_files) {
        if (pair.first > max_file_index) {
            max_file_index = pair.first;
        }
    }

    if (max_file_index < RANGE_START) {
        std::cout << "No files found in the specified range starting from " << RANGE_START << std::endl;
        return;
    }

    // Vector to store all analysis results
    std::vector<AnalysisResult> all_results;

    // Process each sigma threshold
    for (int current_sigma = MIN_SIGMA_THRESHOLD; current_sigma <= MAX_SIGMA_THRESHOLD; current_sigma++) {
        std::cout << "\n" << std::string(80, '=') << std::endl;
        std::cout << "PROCESSING SIGMA THRESHOLD: " << current_sigma << std::endl;
        std::cout << std::string(80, '=') << std::endl;

        double lower_bound = global_mean - current_sigma * global_stddev;
        double upper_bound = global_mean + current_sigma * global_stddev;
        std::cout << "Global " << current_sigma << "-sigma bounds: [" << lower_bound << ", " << upper_bound << "]" << std::endl;

        // Process each range for this sigma threshold
        int current_range_start = RANGE_START;
        int range_number = 1;
        
        while (current_range_start <= max_file_index) {
            std::cout << "\n--- Range " << range_number << ": [" << current_range_start 
                      << ", " << (current_range_start + RANGE_SIZE - 1) << "] ---" << std::endl;
            
            AnalysisResult result = process_range(current_range_start, current_sigma, numeric_files, file_amplitudes, global_mean, global_stddev);
            all_results.push_back(result);
            
            current_range_start += RANGE_SIZE;
            range_number++;
        }
    }

    // Print comprehensive final statistics
    print_final_statistics(all_results, global_mean, global_stddev, total_csv_files, files_processed_successfully, numeric_files.size());
}

// --- Helper Function: Process Single Range ---

AnalysisResult process_range(int range_start, int current_sigma_threshold, 
                            const std::map<int, std::string>& numeric_files,
                            const std::map<int, std::vector<double>>& file_amplitudes,
                            double global_mean, double global_stddev) {
    
    AnalysisResult result;
    result.sigma_threshold = current_sigma_threshold;
    result.range_start = range_start;
    result.range_end = range_start + RANGE_SIZE - 1;
    result.files_analyzed = 0;
    result.files_with_outliers = 0;
    result.total_outliers = 0;
    
    for (int i = range_start; i < range_start + RANGE_SIZE; i++) {
        auto file_it = numeric_files.find(i);
        if (file_it != numeric_files.end()) {
            std::string filename = file_it->second;
            auto amp_it = file_amplitudes.find(i);
            const std::vector<double>& amplitude_data = amp_it->second;
            
            int outliers_in_file = count_outliers_beyond_nsigma(amplitude_data, global_mean, global_stddev, current_sigma_threshold);
            result.total_outliers += outliers_in_file;
            result.files_analyzed++;
            
            if (outliers_in_file > 0) {
                result.files_with_outliers++;
                std::cout << "File " << i << ".csv (" << filename << "): " << outliers_in_file << " outliers" << std::endl;
            } else {
                std::cout << "File " << i << ".csv (" << filename << "): No outliers" << std::endl;
            }
        }
    }

    // Print range results
    std::cout << "Range [" << result.range_start << ", " << result.range_end << "] Results:" << std::endl;
    std::cout << "  Files analyzed: " << result.files_analyzed << std::endl;
    std::cout << "  Files with outliers: " << result.files_with_outliers << std::endl;
    std::cout << "  Total outliers beyond " << current_sigma_threshold << "-sigma: " << result.total_outliers << std::endl;
    
    return result;
}

// --- Helper Function Implementations ---

/**
 * @brief Extracts numeric index from filename (e.g., "123.csv" -> 123, "data.csv" -> -1).
 * @param filename The filename to parse.
 * @return The numeric index, or -1 if filename is not purely numeric.
 */
int extract_numeric_index(const std::string& filename) {
    // Remove the .csv extension
    std::string name_without_ext = filename;
    size_t dot_pos = name_without_ext.find_last_of('.');
    if (dot_pos != std::string::npos) {
        name_without_ext = name_without_ext.substr(0, dot_pos);
    }
    
    // Check if the remaining string is purely numeric
    if (name_without_ext.empty()) {
        return -1;
    }
    
    for (char c : name_without_ext) {
        if (!std::isdigit(c)) {
            return -1;
        }
    }
    
    // Convert to integer
    try {
        return std::stoi(name_without_ext);
    } catch (const std::exception&) {
        return -1;
    }
}

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
 * @brief Counts the number of outliers beyond n sigma in a dataset using global statistics.
 * @param data The input data vector.
 * @param global_mean The mean calculated from all files.
 * @param global_stddev The standard deviation calculated from all files.
 * @param sigma_threshold The sigma threshold to use for outlier detection.
 * @return The number of outliers found.
 */
int count_outliers_beyond_nsigma(const std::vector<double>& data, double global_mean, double global_stddev, int sigma_threshold) {
    if (data.empty()) {
        return 0;
    }

    // Check if statistics are valid
    if (!is_valid_number(global_mean) || !is_valid_number(global_stddev) || global_stddev == 0.0) {
        return 0;
    }

    double lower_bound = global_mean - sigma_threshold * global_stddev;
    double upper_bound = global_mean + sigma_threshold * global_stddev;

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

// --- Helper Function: Print Comprehensive Final Statistics ---

void print_final_statistics(const std::vector<AnalysisResult>& results, 
                           double global_mean, double global_stddev,
                           int total_csv_files, int files_processed_successfully,
                           int numeric_files_count) {
    
    std::cout << "\n" << std::string(100, '=') << std::endl;
    std::cout << "COMPREHENSIVE FINAL STATISTICS" << std::endl;
    std::cout << std::string(100, '=') << std::endl;
    
    // Overall file statistics
    std::cout << "\n--- FILE PROCESSING SUMMARY ---" << std::endl;
    std::cout << "Total CSV files found: " << total_csv_files << std::endl;
    std::cout << "Files successfully processed: " << files_processed_successfully << std::endl;
    std::cout << "Files with numeric names found: " << numeric_files_count << std::endl;
    
    // Global statistics
    std::cout << "\n--- GLOBAL STATISTICS (from ALL files) ---" << std::endl;
    std::cout << "Global Mean: " << global_mean << std::endl;
    std::cout << "Global Standard Deviation: " << global_stddev << std::endl;
    
    // Detailed results table
    std::cout << "\n--- DETAILED OUTLIER ANALYSIS RESULTS ---" << std::endl;
    std::cout << "Sigma | Range Start | Range End | Files Analyzed | Files w/ Outliers | Total Outliers" << std::endl;
    std::cout << std::string(85, '-') << std::endl;
    
    for (const auto& result : results) {
        printf("%5d | %11d | %9d | %14d | %17d | %14d\n",
               result.sigma_threshold, result.range_start, result.range_end,
               result.files_analyzed, result.files_with_outliers, result.total_outliers);
    }
    
    // Summary by sigma threshold
    std::cout << "\n--- SUMMARY BY SIGMA THRESHOLD ---" << std::endl;
    for (int sigma = MIN_SIGMA_THRESHOLD; sigma <= MAX_SIGMA_THRESHOLD; sigma++) {
        int total_files_analyzed = 0;
        int total_files_with_outliers = 0;
        int total_outliers = 0;
        int ranges_processed = 0;
        
        for (const auto& result : results) {
            if (result.sigma_threshold == sigma) {
                total_files_analyzed += result.files_analyzed;
                total_files_with_outliers += result.files_with_outliers;
                total_outliers += result.total_outliers;
                ranges_processed++;
            }
        }
        
        std::cout << sigma << "-Sigma Threshold:" << std::endl;
        std::cout << "  Ranges processed: " << ranges_processed << std::endl;
        std::cout << "  Total files analyzed: " << total_files_analyzed << std::endl;
        std::cout << "  Total files with outliers: " << total_files_with_outliers << std::endl;
        std::cout << "  Total outliers found: " << total_outliers << std::endl;
        if (total_files_analyzed > 0) {
            std::cout << "  Percentage of files with outliers: " 
                      << (100.0 * total_files_with_outliers / total_files_analyzed) << "%" << std::endl;
        }
        std::cout << std::endl;
    }
    
    // Summary by range (across all sigma thresholds)
    std::cout << "--- SUMMARY BY RANGE (across all sigma thresholds) ---" << std::endl;
    std::map<std::pair<int,int>, std::vector<AnalysisResult>> range_groups;
    
    // Group results by range
    for (const auto& result : results) {
        std::pair<int,int> range_key = {result.range_start, result.range_end};
        range_groups[range_key].push_back(result);
    }
    
    for (const auto& range_group : range_groups) {
        int range_start = range_group.first.first;
        int range_end = range_group.first.second;
        const std::vector<AnalysisResult>& range_results = range_group.second;
        
        std::cout << "Range [" << range_start << ", " << range_end << "]:" << std::endl;
        std::cout << "  Files in range: " << (range_results.empty() ? 0 : range_results[0].files_analyzed) << std::endl;
        
        for (const auto& result : range_results) {
            std::cout << "  " << result.sigma_threshold << "-sigma: " 
                      << result.total_outliers << " outliers in " 
                      << result.files_with_outliers << "/" << result.files_analyzed << " files" << std::endl;
        }
        std::cout << std::endl;
    }
    
    // Grand totals
    std::cout << "--- GRAND TOTALS ---" << std::endl;
    int grand_total_outliers = 0;
    int grand_total_analyses = results.size();
    
    for (const auto& result : results) {
        grand_total_outliers += result.total_outliers;
    }
    
    std::cout << "Total analyses performed: " << grand_total_analyses << std::endl;
    std::cout << "Grand total outliers found (all sigma thresholds, all ranges): " << grand_total_outliers << std::endl;
    
    std::cout << "\n" << std::string(100, '=') << std::endl;
}