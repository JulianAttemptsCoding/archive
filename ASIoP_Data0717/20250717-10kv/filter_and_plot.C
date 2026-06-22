//
// File: filter_and_plot_csv.C
//
// Description:
// This ROOT C++ macro scans the current directory for CSV files (.csv),
// reads specific columns, filters the data by removing points
// within 3 standard deviations of the mean, and then plots the result
// as a histogram with logarithmic y-axis and full domain x-axis.
// This version includes comprehensive debugging and data validation.
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
//    .L filter_and_plot_csv.C+
//    process_csv_files()
//
// The '+' after the macro name tells ROOT to compile it using ACLiC,
// which is necessary for more complex scripts like this one.
//

#include <TSystem.h>
#include <TString.h>
#include <TGraph.h>
#include <TH1F.h>
#include <TCanvas.h>
#include <TStyle.h>
#include <TAxis.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <numeric>
#include <fstream>   // For CSV file reading
#include <string>    // For string manipulation
#include <sstream>   // For parsing lines
#include <algorithm> // For min_element, max_element
#include <limits>    // For numeric limits

// --- Function Declarations ---

// Main function to be called from ROOT
void process_csv_files();

// Helper function to read data from a single CSV file
bool read_csv_data(const char* filename, std::vector<double>& time, std::vector<double>& amplitude);

// Helper function to calculate statistics (mean and standard deviation)
void calculate_stats(const std::vector<double>& data, double& mean, double& stddev);

// Helper function to validate numeric values
bool is_valid_number(double value);

// Helper function to trim whitespace from strings
std::string trim(const std::string& str);

// Helper function to safely parse string to double
bool safe_string_to_double(const std::string& str, double& result);

// --- Main Processing Function ---

void process_csv_files() {
    std::cout << "Starting CSV file processing..." << std::endl;

    // Vectors to store the aggregated data from all files
    std::vector<double> all_time;
    std::vector<double> all_amplitude;

    // Counters for debugging
    int total_files_processed = 0;
    int successful_files = 0;
    int total_rows_processed = 0;
    int valid_data_points = 0;
    int invalid_data_points = 0;

    // Get the current directory
    TString current_dir = gSystem->pwd();
    std::cout << "Current directory: " << current_dir << std::endl;
    
    void* dir_handle = gSystem->OpenDirectory(current_dir);

    if (!dir_handle) {
        std::cerr << "Error: Could not open current directory." << std::endl;
        return;
    }

    const char* entry;
    // Loop through all files in the directory
    while ((entry = gSystem->GetDirEntry(dir_handle))) {
        TString filename(entry);
        // Filter for .csv files
        if (filename.EndsWith(".csv")) {
            std::cout << "Processing CSV file: " << filename << std::endl;
            total_files_processed++;
            
            std::vector<double> file_time, file_amplitude;
            if (read_csv_data(filename.Data(), file_time, file_amplitude)) {
                successful_files++;
                all_time.insert(all_time.end(), file_time.begin(), file_time.end());
                all_amplitude.insert(all_amplitude.end(), file_amplitude.begin(), file_amplitude.end());
                std::cout << "  -> Read " << file_amplitude.size() << " valid data points from " << filename << std::endl;
                valid_data_points += file_amplitude.size();
            } else {
                std::cerr << "  -> Failed to read data from " << filename << std::endl;
            }
        }
    }
    gSystem->FreeDirectory(dir_handle);

    std::cout << "\n=== DATA PROCESSING SUMMARY ===" << std::endl;
    std::cout << "Total CSV files found: " << total_files_processed << std::endl;
    std::cout << "Successfully processed files: " << successful_files << std::endl;
    std::cout << "Total valid data points: " << valid_data_points << std::endl;
    std::cout << "Total invalid data points skipped: " << invalid_data_points << std::endl;

    if (all_amplitude.empty()) {
        std::cerr << "Error: No valid data was successfully read from any CSV files. Aborting." << std::endl;
        return;
    }

    // --- Data Quality Check ---
    std::cout << "\n=== DATA QUALITY CHECK ===" << std::endl;
    
    // Check for any remaining invalid values
    int remaining_invalid = 0;
    for (const auto& amp : all_amplitude) {
        if (!is_valid_number(amp)) {
            remaining_invalid++;
        }
    }
    
    if (remaining_invalid > 0) {
        std::cerr << "Warning: Found " << remaining_invalid << " invalid values that passed initial filtering!" << std::endl;
    }

    // --- Store the full input domain bounds ---
    double input_min = *std::min_element(all_amplitude.begin(), all_amplitude.end());
    double input_max = *std::max_element(all_amplitude.begin(), all_amplitude.end());
    
    std::cout << "Full input domain: [" << input_min << ", " << input_max << "]" << std::endl;
    std::cout << "Domain range: " << (input_max - input_min) << std::endl;

    // Check if domain is reasonable
    if (!is_valid_number(input_min) || !is_valid_number(input_max)) {
        std::cerr << "Error: Input domain contains invalid values. Cannot proceed." << std::endl;
        return;
    }

    if (input_min == input_max) {
        std::cerr << "Error: All data points have the same value. Cannot create meaningful histogram." << std::endl;
        return;
    }

    // --- 3-Sigma Filtering ---
    std::cout << "\n=== STATISTICAL ANALYSIS ===" << std::endl;
    
    double mean, stddev;
    calculate_stats(all_amplitude, mean, stddev);
    
    std::cout << "Calculated Mean: " << mean << std::endl;
    std::cout << "Calculated StdDev (sigma): " << stddev << std::endl;

    // Check if statistics are valid
    if (!is_valid_number(mean) || !is_valid_number(stddev)) {
        std::cerr << "Error: Statistical calculations failed. Data may contain invalid values." << std::endl;
        return;
    }

    if (stddev == 0.0) {
        std::cerr << "Error: Standard deviation is zero. All data points are identical." << std::endl;
        return;
    }

    double lower_bound = mean - 3 * stddev;
    double upper_bound = mean + 3 * stddev;
    std::cout << "3-Sigma filter range (data to be removed): [" << lower_bound << ", " << upper_bound << "]" << std::endl;

    std::vector<double> filtered_amplitude;
    int points_in_range = 0;
    int points_out_of_range = 0;
    
    for (const auto& amp : all_amplitude) {
        // Keep data points that are OUTSIDE the 3-sigma range
        if (amp < lower_bound || amp > upper_bound) {
            filtered_amplitude.push_back(amp);
            points_out_of_range++;
        } else {
            points_in_range++;
        }
    }

    std::cout << "Data points within 3-sigma (removed): " << points_in_range << std::endl;
    std::cout << "Data points outside 3-sigma (kept): " << points_out_of_range << std::endl;
    std::cout << "Fraction of data kept: " << (double)points_out_of_range / all_amplitude.size() * 100 << "%" << std::endl;

    if (filtered_amplitude.empty()) {
        std::cerr << "Error: All data was removed by the 3-sigma filter. Cannot create histogram." << std::endl;
        std::cerr << "This might indicate that your data is very well-behaved (low noise) or there's an issue with the filtering logic." << std::endl;
        return;
    }

    // --- Histogram Creation and Plotting ---
    std::cout << "\n=== HISTOGRAM CREATION ===" << std::endl;
    
    gStyle->SetOptStat(1111); // Show statistics on the plot

    TCanvas* canvas = new TCanvas("c1", "Filtered Amplitude Histogram", 800, 600);
    
    // Set logarithmic y-axis
    canvas->SetLogy();
    
    // Add some padding to the domain for better visualization
    double domain_padding = (input_max - input_min) * 0.05; // 5% padding
    double hist_min = input_min - domain_padding;
    double hist_max = input_max + domain_padding;
    
    std::cout << "Histogram x-axis range: [" << hist_min << ", " << hist_max << "]" << std::endl;
    
    // Use the full input domain for x-axis bounds, not just the filtered data
    TH1F* hist = new TH1F("hist_amplitude", "Filtered Amplitude Distribution;Amplitude;Counts", 100, hist_min, hist_max);

    // Fill histogram
    for (const auto& amp : filtered_amplitude) {
        hist->Fill(amp);
    }

    // Check if histogram has any entries
    if (hist->GetEntries() == 0) {
        std::cerr << "Error: Histogram has no entries!" << std::endl;
        delete hist;
        delete canvas;
        return;
    }

    std::cout << "Histogram entries: " << hist->GetEntries() << std::endl;
    std::cout << "Histogram integral: " << hist->Integral() << std::endl;

    hist->GetXaxis()->SetTitle("Amplitude");
    hist->GetYaxis()->SetTitle("Frequency (Counts) [Log Scale]");
    hist->SetFillColor(kBlue - 9);
    hist->SetLineColor(kBlue);
    
    // Set minimum value for log scale to avoid issues with zero counts
    hist->SetMinimum(0.1);
    
    hist->Draw();

    canvas->Update();
    canvas->SaveAs("filtered_amplitude_histogram.png");
    std::cout << "Histogram saved as filtered_amplitude_histogram.png" << std::endl;
    std::cout << "X-axis covers full input domain with padding: [" << hist_min << ", " << hist_max << "]" << std::endl;
    std::cout << "Y-axis is logarithmic" << std::endl;
    
    std::cout << "\n=== PROCESSING COMPLETE ===" << std::endl;
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
                if (invalid_rows <= 5) { // Only show first 5 errors to avoid spam
                    std::cerr << "  Warning: Invalid data in " << filename << " line " << current_line_num 
                              << " (time=" << current_time << ", amp_sum=" << current_amplitude_sum << ")" << std::endl;
                }
            }
        } else {
            invalid_rows++;
            if (invalid_rows <= 5) { // Only show first 5 errors to avoid spam
                std::cerr << "  Warning: Insufficient columns in " << filename << " line " << current_line_num 
                          << " (found " << row_values.size() << ", need 5)" << std::endl;
            }
        }
    }

    file.close();
    
    if (invalid_rows > 5) {
        std::cerr << "  ... and " << (invalid_rows - 5) << " more invalid rows in " << filename << std::endl;
    }
    
    std::cout << "  File " << filename << ": " << valid_rows << " valid rows, " << invalid_rows << " invalid rows" << std::endl;
    
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