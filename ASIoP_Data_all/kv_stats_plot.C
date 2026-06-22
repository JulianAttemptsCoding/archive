//
// File: kv_stats_plot.C
//
// Description:
// This ROOT C++ macro scans the current directory for CSV files (.csv).
// It reads specific columns (sums columns C+D+E) from row 22 onwards.
// It categorizes files based on the 3rd digit of their numeric name:
//   - 3rd digit '0' -> 8kV, '1' -> 9kV, ..., '6' -> 14kV.
//   - Files with < 3 digits are treated as having a 3rd digit of '0'.
// For each kV category (8-14kV), it calculates the mean and standard deviation
// using data specifically from rows 22-98 of files within that category only.
// Finally, it plots Standard Deviation (Sigma) vs. kV and Mean vs. kV on separate graphs.
//
// Dependencies:
// This script uses standard C++ file I/O for CSV parsing.
// No external library like libxls is required for CSV.
//
// How to Run in ROOT (Ubuntu):
// 1. Launch the ROOT interactive terminal:
//    root
//
// 2. Compile and execute this macro from the ROOT prompt:
//    .L kv_stats_plot.C+
//    kv_stats_plot()
//
// The '+' after the macro name tells ROOT to compile it using ACLiC.
//

#include <TSystem.h>
#include <TString.h>
#include <TCanvas.h>
#include <TGraphErrors.h>
#include <TLegend.h>
#include <TAxis.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <fstream>   // For CSV file reading
#include <string>    // For string manipulation
#include <sstream>   // For parsing lines
#include <algorithm> // For sorting
#include <limits>    // For numeric limits
#include <map>       // For storing filename to index mapping and kV stats
#include <set>       // For storing unique kV categories

// --- Function Declarations ---
// Structure to store calculated statistics per category
struct KVStats {
    int kV; // 8 to 14
    double mean;
    double stddev;
    int data_points; // Number of data points used for calculation
    int file_count;  // Number of files contributing to the stats
};

// Main function to be called from ROOT
void kv_stats_plot();

// Helper function to read data from a single CSV file (reads from row 22 onwards)
bool read_csv_data(const char* filename, std::vector<double>& time, std::vector<double>& amplitude);

// Helper function to calculate statistics (mean and standard deviation)
void calculate_stats(const std::vector<double>& data, double& mean, double& stddev);

// Helper function to validate numeric values
bool is_valid_number(double value);

// Helper function to trim whitespace from strings
std::string trim(const std::string& str);

// Helper function to safely parse string to double
bool safe_string_to_double(const std::string& str, double& result);

// Helper function to extract numeric index from filename
int extract_numeric_index(const std::string& filename);

// Helper function to extract the kV category from the filename's numeric index
int extract_kV_category(int file_index);

// --- Main Processing Function ---
void kv_stats_plot() {
    std::cout << "Starting CSV file analysis to calculate per-kV Mean and Sigma..." << std::endl;
    std::cout << "Statistics calculated using rows 22-98 for each kV category separately." << std::endl;
    std::cout << "kV categories determined by the 3rd digit of the numeric filename (0->8kV, 1->9kV, ..., 6->14kV)." << std::endl;

    // Map to store filename to data mapping (full data from row 22 onwards)
    std::map<int, std::string> numeric_files; // index -> filename
    std::map<int, std::vector<double>> file_amplitudes; // index -> amplitude data (row 22 onwards)

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

    // FIRST PASS: Read all CSV files with numeric names and collect data
    std::cout << "\n=== FIRST PASS: Reading CSV files with numeric names ===" << std::endl;
    while ((entry = gSystem->GetDirEntry(dir_handle))) {
        TString filename(entry);
        // Filter for .csv files
        if (filename.EndsWith(".csv")) {
            total_csv_files++;
            int file_index = extract_numeric_index(filename.Data());
            if (file_index >= 0) { // Only process files with numeric names
                std::cout << "Reading CSV file: " << filename << " (index: " << file_index << ")" << std::endl;
                std::vector<double> file_time, file_amplitude_full; // Stores all data from row 22 onwards
                if (read_csv_data(filename.Data(), file_time, file_amplitude_full)) {
                    files_processed_successfully++;
                    // Store full data for files with numeric names
                    numeric_files[file_index] = filename.Data();
                    file_amplitudes[file_index] = file_amplitude_full;
                } else {
                    std::cerr << "  -> Failed to read data from " << filename << std::endl;
                }
            }
        }
    }
    gSystem->FreeDirectory(dir_handle);

    if (numeric_files.empty()) {
        std::cerr << "Error: No CSV files with numeric names were found or successfully read." << std::endl;
        return;
    }

    std::cout << "\nSuccessfully processed " << files_processed_successfully << " files with numeric names." << std::endl;

    // --- Prepare for Analysis by Category ---
    // Map to store amplitude data for each kV category (from rows 22-98)
    std::map<int, std::vector<double>> kV_amplitude_data; // kV -> data points (rows 22-98)
    // Map to count files per kV category
    std::map<int, int> kV_file_count; // kV -> number of files

    // SECOND PASS: Aggregate data by kV category
    std::cout << "\n=== SECOND PASS: Aggregating data by kV category (using rows 22-98) ===" << std::endl;
    for (const auto& file_pair : numeric_files) {
        int file_index = file_pair.first;
        int kV_cat = extract_kV_category(file_index);

        if (kV_cat >= 8 && kV_cat <= 14) {
            auto amp_it = file_amplitudes.find(file_index);
            if (amp_it != file_amplitudes.end()) {
                const std::vector<double>& file_data_full = amp_it->second; // Data from row 22 onwards

                // Extract data corresponding to rows 22-98 (indices 0-76 in file_data_full)
                const int STATS_ROWS_START_IDX = 0; // Relative to data starting from row 22
                const int STATS_ROWS_END_IDX = 97 - 21; // 76, inclusive index in file_data_full

                if (!file_data_full.empty()) {
                    int stats_end_idx_clamped = std::min(STATS_ROWS_END_IDX, static_cast<int>(file_data_full.size()) - 1);
                    if (stats_end_idx_clamped >= STATS_ROWS_START_IDX) {
                        // Add the subset of data (rows 22-98 equivalent) to the kV category's data
                        kV_amplitude_data[kV_cat].insert(
                            kV_amplitude_data[kV_cat].end(),
                            file_data_full.begin() + STATS_ROWS_START_IDX,
                            file_data_full.begin() + stats_end_idx_clamped + 1 // +1 for inclusive end
                        );
                        kV_file_count[kV_cat]++;
                        std::cout << "Added data from file " << file_index << " (kV " << kV_cat << "). "
                                  << "Used " << (stats_end_idx_clamped - STATS_ROWS_START_IDX + 1)
                                  << " points (rows 22-" << (22 + stats_end_idx_clamped) << ")." << std::endl;
                    } else {
                        std::cout << "File " << file_index << " (kV " << kV_cat << ") has insufficient data for rows 22-98." << std::endl;
                    }
                } else {
                    std::cout << "File " << file_index << " (kV " << kV_cat << ") has no valid data." << std::endl;
                }
            } else {
                 std::cerr << "Warning: Amplitude data not found for file index " << file_index << std::endl;
            }
        } else {
            std::cout << "File " << file_index << " maps to invalid kV category (" << kV_cat << "). Skipping." << std::endl;
        }
    }


    // --- Calculate Statistics for Each kV Category ---
    std::cout << "\n=== CALCULATING PER-kV STATISTICS (using rows 22-98) ===" << std::endl;
    std::vector<KVStats> all_kV_stats;
    std::set<int> present_kVs; // To know which kVs actually have data

    for (int kV = 8; kV <= 14; ++kV) {
        KVStats stats;
        stats.kV = kV;
        stats.mean = 0.0;
        stats.stddev = 0.0;
        stats.data_points = 0;
        stats.file_count = 0;

        auto data_it = kV_amplitude_data.find(kV);
        auto count_it = kV_file_count.find(kV);

        if (data_it != kV_amplitude_data.end() && !data_it->second.empty()) {
            if (count_it != kV_file_count.end()) {
                stats.file_count = count_it->second;
            }
            const std::vector<double>& data_for_kV = data_it->second;
            stats.data_points = data_for_kV.size();
            calculate_stats(data_for_kV, stats.mean, stats.stddev);

            if (!is_valid_number(stats.mean) || !is_valid_number(stats.stddev)) {
                std::cerr << "Warning: Invalid statistics calculated for kV " << kV << ". Skipping." << std::endl;
                continue; // Skip invalid stats
            }

            std::cout << "kV " << kV << "kV:" << std::endl;
            std::cout << "  Files contributing: " << stats.file_count << std::endl;
            std::cout << "  Data points (rows 22-98): " << stats.data_points << std::endl;
            std::cout << "  Mean: " << stats.mean << std::endl;
            std::cout << "  Standard Deviation (Sigma): " << stats.stddev << std::endl;
            std::cout << std::endl;

            all_kV_stats.push_back(stats);
            present_kVs.insert(kV);
        } else {
            std::cout << "kV " << kV << "kV: No data found or empty dataset." << std::endl;
        }
    }

    if (all_kV_stats.empty()) {
        std::cerr << "Error: No valid statistics were calculated for any kV category." << std::endl;
        return;
    }

    // --- Plotting ---
    std::cout << "\n=== GENERATING PLOTS ===" << std::endl;

    // Sort results by kV for consistent plotting
    std::sort(all_kV_stats.begin(), all_kV_stats.end(), [](const KVStats& a, const KVStats& b) {
        return a.kV < b.kV;
    });

    int n_points = all_kV_stats.size();
    std::vector<double> x_vals(n_points);
    std::vector<double> mean_vals(n_points);
    std::vector<double> sigma_vals(n_points);
    std::vector<double> x_errs(n_points, 0.0); // No error on x-axis

    for (int i = 0; i < n_points; ++i) {
        x_vals[i] = static_cast<double>(all_kV_stats[i].kV);
        mean_vals[i] = all_kV_stats[i].mean;
        sigma_vals[i] = all_kV_stats[i].stddev;
        // Note: Simple plot, no y-errors calculated for mean/sigma themselves in this context
    }


    // --- Plot 1: Sigma vs kV ---
    TCanvas *c1 = new TCanvas("c1", "Sigma vs kV", 800, 600);
    c1->SetGrid();

    TGraphErrors *graph_sigma = new TGraphErrors(n_points, x_vals.data(), sigma_vals.data(), x_errs.data(), nullptr); // No y-errors
    graph_sigma->SetTitle("Standard Deviation (Sigma) vs kV; kV; Sigma (Standard Deviation)");
    graph_sigma->SetMarkerStyle(20);
    graph_sigma->SetMarkerSize(1.2);
    graph_sigma->SetMarkerColor(kBlue);
    graph_sigma->SetLineColor(kBlue);
    graph_sigma->SetLineWidth(2);
    graph_sigma->Draw("APL"); // Axis, Points, Line

    // Set reasonable axis limits
    graph_sigma->GetXaxis()->SetLimits(7.5, 14.5);
    // Y-axis range might need adjustment based on your data
    auto minmax_sigma = std::minmax_element(sigma_vals.begin(), sigma_vals.end());
    if (minmax_sigma.first != minmax_sigma.second) {
        double range_sigma = *minmax_sigma.second - *minmax_sigma.first;
        graph_sigma->GetYaxis()->SetRangeUser(*minmax_sigma.first - 0.1 * range_sigma, *minmax_sigma.second + 0.1 * range_sigma);
    }

    c1->Update();
    std::cout << "Plot 'c1' created: Sigma vs kV. You can interact with it or save it via the ROOT canvas menu." << std::endl;


     // --- Plot 2: Mean vs kV ---
    TCanvas *c2 = new TCanvas("c2", "Mean vs kV", 800, 600);
    c2->SetGrid();

    TGraphErrors *graph_mean = new TGraphErrors(n_points, x_vals.data(), mean_vals.data(), x_errs.data(), nullptr); // No y-errors
    graph_mean->SetTitle("Mean vs kV; kV; Mean");
    graph_mean->SetMarkerStyle(21); // Different marker
    graph_mean->SetMarkerSize(1.2);
    graph_mean->SetMarkerColor(kRed);
    graph_mean->SetLineColor(kRed);
    graph_mean->SetLineWidth(2);
    graph_mean->Draw("APL"); // Axis, Points, Line

    // Set reasonable axis limits
    graph_mean->GetXaxis()->SetLimits(7.5, 14.5);
    // Y-axis range might need adjustment based on your data
    auto minmax_mean = std::minmax_element(mean_vals.begin(), mean_vals.end());
    if (minmax_mean.first != minmax_mean.second) {
        double range_mean = *minmax_mean.second - *minmax_mean.first;
        graph_mean->GetYaxis()->SetRangeUser(*minmax_mean.first - 0.1 * range_mean, *minmax_mean.second + 0.1 * range_mean);
    }

    c2->Update();
    std::cout << "Plot 'c2' created: Mean vs kV. You can interact with it or save it via the ROOT canvas menu." << std::endl;

    std::cout << "\nAnalysis and plotting complete." << std::endl;
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
 * @brief Extracts the kV category from the numeric file index.
 *        3rd digit (0-indexed from right) determines the category:
 *        0->8kV, 1->9kV, ..., 6->14kV.
 *        If the number has fewer than 3 digits, the 3rd digit is considered 0.
 * @param file_index The numeric index of the file.
 * @return The kV category (8 to 14), or -1 if the resulting category is invalid.
 */
int extract_kV_category(int file_index) {
    if (file_index < 0) return -1;
    // Convert file_index to string to easily access digits
    std::string index_str = std::to_string(file_index);
    // Get the 3rd digit from the right (0-indexed position 2 from the end)
    int third_digit = 0; // Default if less than 3 digits
    if (index_str.length() >= 3) {
        // Access the character at the correct position from the end
        char third_char = index_str[index_str.length() - 3];
        // Convert char digit to integer
        third_digit = third_char - '0';
    }
    // If length is < 3, third_digit remains 0, which is correct.
    // Map 0->8kV, 1->9kV, ..., 6->14kV
    if (third_digit >= 0 && third_digit <= 6) {
        return 8 + third_digit;
    } else {
        return -1; // Or return a default like 8 (8kV) if preferred
    }
}

/**
 * @brief Reads data from a specified .csv file with comprehensive error handling,
 *        processing rows from 22 onwards (inclusive, 0-indexed 21 onwards) and summing columns C+D+E.
 * @param filename The name of the CSV file.
 * @param time Vector to store time data (Column A - 0-indexed).
 * @param amplitude Vector to store summed amplitude data (Columns C+D+E - 0-indexed 2,3,4).
 * @return true if successful (at least one valid row processed), false otherwise.
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
        // Skip lines until the data start row
        if (current_line_num <= DATA_START_ROW) {
            continue;
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
        // Ensure we have enough columns before accessing (need A, B, C, D, E -> indices 0 to 4)
        if (row_values.size() >= 5) {
            double current_time = row_values[0]; // Column A (0-indexed)
            double current_amplitude_sum = row_values[2] + row_values[3] + row_values[4]; // Columns C, D, E (0-indexed)
            // Validate the calculated values
            if (is_valid_number(current_time) && is_valid_number(current_amplitude_sum)) {
                time.push_back(current_time);
                amplitude.push_back(current_amplitude_sum);
                valid_rows++;
            } else {
                invalid_rows++;
            }
        } else {
            // Not enough columns in this row, consider it invalid
            invalid_rows++;
        }
    }
    file.close();
    if (valid_rows == 0) {
        std::cout << "  -> Warning: No valid data rows found from row " << (DATA_START_ROW + 1) << " onwards in file " << filename << std::endl;
    } else {
        std::cout << "  -> Read " << valid_rows << " valid rows (from row " << (DATA_START_ROW + 1) << " onwards)" << std::endl;
    }
    // Return true if at least one valid row was processed
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
    // Calculate standard deviation (population formula)
    double sq_sum = 0.0;
    for (const auto& x : data) {
        double diff = x - mean;
        sq_sum += diff * diff;
    }
    stddev = std::sqrt(sq_sum / data.size()); // Use N for population std dev, N-1 for sample
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