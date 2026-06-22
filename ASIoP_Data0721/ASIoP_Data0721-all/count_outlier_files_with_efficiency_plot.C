//
// File: count_outlier_files_with_efficiency_plot.C
//
// Description:
// This ROOT C++ macro scans the current directory for CSV files (.csv),
// reads specific columns (sums columns C+D+E) from row 22 onwards.
// Calculates n-sigma statistics based on rows 22-98 combined across ALL files.
// Categorizes files based on the 3rd digit of their numeric name:
//   - 3rd digit '0' -> 8kV, '1' -> 9kV, ..., '6' -> 14kV.
//   - Files with < 3 digits are treated as having a 3rd digit of '0'.
// Counts outliers from files within these categories using data from row 22 onwards.
// Outlier detection uses global statistics (mean/stddev from rows 22-98).
// The analysis is repeated for sigma thresholds from 1 to 5.
// Finally, it plots the efficiency (#files_with_outliers / #files_in_category)
// vs. kV (8 to 14) for each sigma level, including 1-sigma error bars.
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
//    .L count_outlier_files_with_efficiency_plot.C+
//    count_outlier_files_with_efficiency_plot()
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
#include <algorithm> // For min_element, max_element
#include <limits>    // For numeric limits
#include <map>       // For storing filename to index mapping
#include <set>       // For storing unique kV categories

// --- CONFIGURATION ---
const int MIN_SIGMA_THRESHOLD = 1; // Minimum sigma threshold
const int MAX_SIGMA_THRESHOLD = 5; // Maximum sigma threshold

// --- Function Declarations ---
// Structure to store analysis results per category
struct CategoryResult {
    int kV; // 8 to 14
    int total_files;
    int files_with_outliers;
    double efficiency;
    double efficiency_error; // 1-sigma error bar
};

// Main function to be called from ROOT
void count_outlier_files_with_efficiency_plot();

// Helper function to process a single kV category with a given sigma threshold
CategoryResult process_category(int kV_category,
                                const std::map<int, std::string>& numeric_files,
                                const std::map<int, std::vector<double>>& file_amplitudes,
                                double global_mean, double global_stddev,
                                int sigma_threshold);

// Helper function to print comprehensive final statistics
void print_final_statistics(const std::map<int, std::vector<CategoryResult>>& all_results,
                           double global_mean, double global_stddev,
                           int total_csv_files, int files_processed_successfully,
                           int numeric_files_count);

// Helper function to read data from a single CSV file (reads from row 22 onwards)
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

// Helper function to extract the kV category from the filename's numeric index
int extract_kV_category(int file_index);

// --- Main Processing Function ---
void count_outlier_files_with_efficiency_plot() {
    std::cout << "Starting CSV file analysis for kV-category, multi-sigma outliers..." << std::endl;
    std::cout << "Sigma thresholds: " << MIN_SIGMA_THRESHOLD << " to " << MAX_SIGMA_THRESHOLD << std::endl;
    std::cout << "Global stats calculated using rows 22-98. Outlier detection uses rows 22 onwards." << std::endl;
    std::cout << "kV categories determined by the 3rd digit of the numeric filename (0->8kV, 1->9kV, ..., 6->14kV)." << std::endl;

    // Vectors to store ALL data from ALL files for global statistics (only rows 22-98)
    std::vector<double> global_stats_amplitude_data; // Data for calculating global mean/stddev

    // Map to store filename to data mapping (full data from row 22 onwards for outlier detection)
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

    // FIRST PASS: Read all CSV files and collect data
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

            if (file_index >= 0) { // Only process files with numeric names for data and categorization
                 std::vector<double> file_time, file_amplitude_full; // Stores all data from row 22 onwards
                 if (read_csv_data(filename.Data(), file_time, file_amplitude_full)) {
                     files_processed_successfully++;
                     // Store full data for files with numeric names (for outlier detection)
                     numeric_files[file_index] = filename.Data();
                     file_amplitudes[file_index] = file_amplitude_full;

                     // Extract data from rows 22-98 (0-indexed 21-97) for global statistics
                     // Note: file_amplitude_full is already data from row 22 onwards.
                     // So, rows 22-98 in the file correspond to indices 0-76 in file_amplitude_full.
                     const int STATS_ROWS_START_IDX = 0; // Relative to data starting from row 22
                     const int STATS_ROWS_END_IDX = 97 - 21; // 76, inclusive index in file_amplitude_full
                     if (!file_amplitude_full.empty()) {
                          int stats_end_idx_clamped = std::min(STATS_ROWS_END_IDX, static_cast<int>(file_amplitude_full.size()) - 1);
                          if (stats_end_idx_clamped >= STATS_ROWS_START_IDX) {
                              // Add the subset of data (rows 22-98 equivalent) to global stats data
                              global_stats_amplitude_data.insert(
                                  global_stats_amplitude_data.end(),
                                  file_amplitude_full.begin() + STATS_ROWS_START_IDX,
                                  file_amplitude_full.begin() + stats_end_idx_clamped + 1 // +1 for inclusive end
                              );
                              std::cout << "  -> Successfully read " << file_amplitude_full.size() << " data points (rows 22+)."
                                        << " Used " << (stats_end_idx_clamped - STATS_ROWS_START_IDX + 1) << " points (rows 22-"
                                        << (22 + stats_end_idx_clamped) << ") for global stats." << std::endl;
                          } else {
                              std::cout << "  -> Successfully read " << file_amplitude_full.size() << " data points (rows 22+),"
                                        << " but file has fewer than " << (22 + STATS_ROWS_START_IDX) << " rows. No data added to global stats for this file." << std::endl;
                          }
                     } else {
                          std::cout << "  -> Successfully read file, but no valid data points extracted (rows 22+)." << std::endl;
                     }
                 } else {
                     std::cerr << "  -> Failed to read data from " << filename << std::endl;
                 }
            } else { // Non-numeric filename
                std::cout << "  -> Skipping non-numeric filename for data analysis." << std::endl;
            }
        }
    }
    gSystem->FreeDirectory(dir_handle);

    if (global_stats_amplitude_data.empty()) {
        std::cerr << "Error: No valid data was read from rows 22-98 of any CSV files for global statistics." << std::endl;
        return;
    }

    // Calculate global statistics from rows 22-98 of ALL files
    std::cout << "\n=== CALCULATING GLOBAL STATISTICS (using rows 22-98) ===" << std::endl;
    std::cout << "Total data points from rows 22-98 across all files: " << global_stats_amplitude_data.size() << std::endl;
    double global_mean, global_stddev;
    calculate_stats(global_stats_amplitude_data, global_mean, global_stddev);
    std::cout << "Global Mean (rows 22-98): " << global_mean << std::endl;
    std::cout << "Global Standard Deviation (rows 22-98): " << global_stddev << std::endl;
    if (!is_valid_number(global_mean) || !is_valid_number(global_stddev) || global_stddev == 0.0) {
        std::cerr << "Error: Invalid global statistics calculated." << std::endl;
        return;
    }

    // --- Prepare for Analysis by Category ---
    // Determine the set of kV categories present
    std::set<int> kV_categories_present;
    for (const auto& pair : numeric_files) {
        int kV_cat = extract_kV_category(pair.first);
        if (kV_cat >= 8 && kV_cat <= 14) { // Only consider valid kV categories
             kV_categories_present.insert(kV_cat);
        }
    }

    if (kV_categories_present.empty()) {
        std::cout << "No files found with numeric names for valid kV categorization (8-14kV)." << std::endl;
        return;
    }

    std::cout << "\nFound kV categories: ";
    for (int kV : kV_categories_present) {
         std::cout << kV << " ";
    }
    std::cout << std::endl;


    // Map to store all analysis results: sigma_threshold -> vector of CategoryResults
    std::map<int, std::vector<CategoryResult>> all_results;

    // Process each sigma threshold
    for (int current_sigma = MIN_SIGMA_THRESHOLD; current_sigma <= MAX_SIGMA_THRESHOLD; current_sigma++) {
        std::cout << "\n" << std::string(80, '=') << std::endl;
        std::cout << "PROCESSING SIGMA THRESHOLD: " << current_sigma << std::endl;
        std::cout << std::string(80, '=') << std::endl;
        double lower_bound = global_mean - current_sigma * global_stddev;
        double upper_bound = global_mean + current_sigma * global_stddev;
        std::cout << "Global " << current_sigma << "-sigma bounds (based on rows 22-98): [" << lower_bound << ", " << upper_bound << "]" << std::endl;

        std::vector<CategoryResult> results_for_this_sigma;
        // Process each kV category for this sigma threshold
        for (int kV_cat : kV_categories_present) {
             std::cout << "\n--- kV Category: " << kV_cat << "kV ---" << std::endl;
             CategoryResult result = process_category(kV_cat, numeric_files, file_amplitudes, global_mean, global_stddev, current_sigma);
             results_for_this_sigma.push_back(result);
        }
        all_results[current_sigma] = results_for_this_sigma;
    }


    // Print comprehensive final statistics
    print_final_statistics(all_results, global_mean, global_stddev, total_csv_files, files_processed_successfully, numeric_files.size());


    // --- Plotting ---
    std::cout << "\n=== GENERATING EFFICIENCY PLOT ===" << std::endl;
    
    // --- CONFIGURATION for Plot ---
    const std::string GRAPH_TITLE = "0721 DATA: Efficieny vs kV"; // Default title, can be changed

    // Create a canvas
    TCanvas *c1 = new TCanvas("c1", "Outlier Efficiency vs kV", 800, 600);
    c1->SetGrid();

    // Create a legend
    TLegend *legend = new TLegend(0.15, 0.7, 0.35, 0.85);
    legend->SetHeader("Sigma Thresholds", "C"); // Centered header

    // Prepare vectors for graph data
    std::vector<TGraphErrors*> graphs;

    // Define colors for different sigma levels (ROOT predefined colors)
    int colors[] = {kBlack, kRed, kBlue, kGreen+2, kMagenta};
    int color_index = 0;

    // Loop through each sigma threshold to create a graph
    for (int sigma = MIN_SIGMA_THRESHOLD; sigma <= MAX_SIGMA_THRESHOLD; sigma++) {
        auto it = all_results.find(sigma);
        if (it == all_results.end() || it->second.empty()) {
            std::cerr << "Warning: No results found for sigma " << sigma << " for plotting." << std::endl;
            continue;
        }

        const std::vector<CategoryResult>& results = it->second;
        int n_points = results.size();

        // Sort results by kV for consistent plotting
        std::vector<CategoryResult> sorted_results = results;
        std::sort(sorted_results.begin(), sorted_results.end(), [](const CategoryResult& a, const CategoryResult& b) {
            return a.kV < b.kV;
        });

        std::vector<double> x_vals(n_points);
        std::vector<double> y_vals(n_points);
        std::vector<double> x_errs(n_points, 0.0); // No error on x-axis
        std::vector<double> y_errs(n_points);

        for (int i = 0; i < n_points; ++i) {
            x_vals[i] = static_cast<double>(sorted_results[i].kV);
            y_vals[i] = sorted_results[i].efficiency;
            y_errs[i] = sorted_results[i].efficiency_error;
        }

        // Create TGraphErrors with lines and markers: "APL" (A=Axis, P=Points, L=Line)
        TGraphErrors *graph = new TGraphErrors(n_points, x_vals.data(), y_vals.data(), x_errs.data(), y_errs.data());
        graph->SetTitle((GRAPH_TITLE + ";kV;Efficiency (# Files with Outliers / # Files)").c_str()); // Set title and axes labels
        graph->SetMarkerStyle(20 + sigma); // Different marker for each sigma
        graph->SetMarkerSize(1.2);
        graph->SetLineColor(colors[color_index % (sizeof(colors)/sizeof(colors[0]))]);
        graph->SetMarkerColor(colors[color_index % (sizeof(colors)/sizeof(colors[0]))]);
        // Set line width for better visibility
        graph->SetLineWidth(2);

        graphs.push_back(graph);

        legend->AddEntry(graph, Form("%d #sigma", sigma), "lp"); // "l" for line, "p" for point in legend
        color_index++;
    }

    // Draw the first graph to set up axes
    if (!graphs.empty()) {
        // Use "APL" to draw Axis, Points, and Lines.
        graphs[0]->Draw("APL"); // Axis, Points, Line
        // The title is set in the graph's title string above
        graphs[0]->GetXaxis()->SetLimits(7.5, 14.5); // Slightly wider than 8-14
        graphs[0]->GetYaxis()->SetRangeUser(0.0, 1.05); // Efficiency from 0 to 1

        // Draw the rest of the graphs on the same canvas
        for (size_t i = 1; i < graphs.size(); ++i) {
            // Use "PL" to draw Points and Lines on Same axes.
            graphs[i]->Draw("PL SAME"); // Points, Line on Same axes
        }

        // Draw the legend
        legend->Draw();

        // Update the canvas to apply changes
        c1->Update();

        std::cout << "Plot 'c1' created with title '" << GRAPH_TITLE << "'. Efficiency vs kV for sigma 1-5 with connected points and 1-sigma error bars." << std::endl;
        std::cout << "You can interact with the plot in the ROOT canvas window." << std::endl;
        std::cout << "To save the plot, you can use the canvas menu: File -> Save As..." << std::endl;

    } else {
        std::cerr << "Error: No graphs were created for plotting." << std::endl;
    }

    // Note: Canvas 'c1', graphs, and legend are managed by ROOT's memory system.
    // They will persist until explicitly deleted or ROOT session ends.
}


// --- Helper Function: Process Single kV Category ---
CategoryResult process_category(int kV_category,
                                const std::map<int, std::string>& numeric_files,
                                const std::map<int, std::vector<double>>& file_amplitudes,
                                double global_mean, double global_stddev,
                                int sigma_threshold) {

    CategoryResult result;
    result.kV = kV_category;
    result.total_files = 0;
    result.files_with_outliers = 0;
    result.efficiency = 0.0;
    result.efficiency_error = 0.0;

    // Iterate through all numeric files to find those in the current kV category
    for (const auto& file_pair : numeric_files) {
        int file_index = file_pair.first;
        int file_kV = extract_kV_category(file_index);

        if (file_kV == kV_category) {
            std::string filename = file_pair.second;
            auto amp_it = file_amplitudes.find(file_index);

            if (amp_it != file_amplitudes.end()) {
                // Use the full amplitude data (from row 22 onwards) for outlier detection in this file
                const std::vector<double>& amplitude_data_full = amp_it->second;
                // Count outliers using the full data but global stats
                int outliers_in_file = count_outliers_beyond_nsigma(amplitude_data_full, global_mean, global_stddev, sigma_threshold);

                result.total_files++;
                if (outliers_in_file > 0) {
                    result.files_with_outliers++;
                    std::cout << "File " << file_index << ".csv (" << filename << "): " << outliers_in_file << " outliers (using data from row 22 onwards)" << std::endl;
                } else {
                    std::cout << "File " << file_index << ".csv (" << filename << "): No outliers (using data from row 22 onwards)" << std::endl;
                }
            } else {
                 std::cerr << "Warning: Amplitude data not found for file index " << file_index << std::endl;
            }
        }
    }

    // Calculate efficiency and error
    if (result.total_files > 0) {
        result.efficiency = static_cast<double>(result.files_with_outliers) / static_cast<double>(result.total_files);

        // Calculate 1-sigma error bar for binomial efficiency (Gaussian approximation)
        // Error = sqrt(p*(1-p)/N) where p is efficiency, N is total files
        double p = result.efficiency;
        double N = static_cast<double>(result.total_files);
        if (N > 0) {
            result.efficiency_error = std::sqrt( (p * (1.0 - p)) / N );
        } else {
            result.efficiency_error = 0.0;
        }
    } else {
        result.efficiency = 0.0;
        result.efficiency_error = 0.0;
        std::cout << "No files found for kV category " << kV_category << "." << std::endl;
    }

    // Print category results
    std::cout << "kV " << result.kV << "kV Results:" << std::endl;
    std::cout << "  Files analyzed: " << result.total_files << std::endl;
    std::cout << "  Files with outliers: " << result.files_with_outliers << std::endl;
    std::cout << "  Efficiency: " << result.efficiency << " +/- " << result.efficiency_error << " (1 sigma)" << std::endl;
    std::cout << "  Total outliers beyond " << sigma_threshold << "-sigma (based on rows 22-98 stats): " << "N/A (counted per file)" << std::endl; // Not directly available here

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
        // Handle cases where 3rd digit is > 6 if necessary, or treat as invalid
        // Based on description, valid range is 0-6.
        // std::cerr << "Warning: File index " << file_index << " has 3rd digit " << third_digit << ", which maps outside 8-14kV range." << std::endl;
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
    // Note: No DATA_END_ROW, so it reads until the end of the file
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
                row_values.push_back(0.0); // Use 0.0 for invalid entries, or consider skipping the row
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
 * @param data The input data vector (e.g., from one file, rows 22 onwards).
 * @param global_mean The mean calculated from rows 22-98 of all files.
 * @param global_stddev The standard deviation calculated from rows 22-98 of all files.
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
void print_final_statistics(const std::map<int, std::vector<CategoryResult>>& all_results, // Changed type
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
    std::cout << "\n--- GLOBAL STATISTICS (calculated from rows 22-98) ---" << std::endl;
    std::cout << "Global Mean: " << global_mean << std::endl;
    std::cout << "Global Standard Deviation: " << global_stddev << std::endl;

    // Detailed results table by kV category and sigma
    std::cout << "\n--- DETAILED EFFICIENCY ANALYSIS RESULTS ---" << std::endl;
    std::cout << "(Outlier detection uses data from row 22 onwards per file)" << std::endl;
    std::cout << "Sigma | kV Category | Files Analyzed | Files w/ Outliers | Efficiency | Error (1 sigma)" << std::endl;
    std::cout << std::string(85, '-') << std::endl;
    for (const auto& sigma_pair : all_results) {
        int sigma = sigma_pair.first;
        const std::vector<CategoryResult>& results = sigma_pair.second;
        // Sort results by kV for consistent printing within each sigma block
        std::vector<CategoryResult> sorted_results = results;
        std::sort(sorted_results.begin(), sorted_results.end(), [](const CategoryResult& a, const CategoryResult& b) {
            return a.kV < b.kV;
        });
        for (const auto& result : sorted_results) {
            printf("%5d | %11dkV | %14d | %17d | %10.4f | %10.4f\n",
                   sigma, result.kV, result.total_files, result.files_with_outliers,
                   result.efficiency, result.efficiency_error);
        }
    }

    // Summary by sigma threshold (aggregated across all kV)
    std::cout << "\n--- SUMMARY BY SIGMA THRESHOLD (aggregated across all kV) ---" << std::endl;
    for (int sigma = MIN_SIGMA_THRESHOLD; sigma <= MAX_SIGMA_THRESHOLD; sigma++) {
        auto it = all_results.find(sigma);
        if (it != all_results.end()) {
            const std::vector<CategoryResult>& results = it->second;
            int total_files_analyzed = 0;
            int total_files_with_outliers = 0;
            for (const auto& res : results) {
                total_files_analyzed += res.total_files;
                total_files_with_outliers += res.files_with_outliers;
            }
            double overall_efficiency = (total_files_analyzed > 0) ?
                                        (static_cast<double>(total_files_with_outliers) / static_cast<double>(total_files_analyzed)) :
                                        0.0;
            std::cout << sigma << "-Sigma Threshold:" << std::endl;
            std::cout << "  Total files analyzed (all kV): " << total_files_analyzed << std::endl;
            std::cout << "  Total files with outliers (all kV): " << total_files_with_outliers << std::endl;
            std::cout << "  Overall Efficiency (all kV): " << overall_efficiency << std::endl;
            if (total_files_analyzed > 0) {
                std::cout << "  Percentage of files with outliers (all kV): "
                          << (100.0 * total_files_with_outliers / total_files_analyzed) << "%" << std::endl;
            }
            std::cout << std::endl;
        }
    }

    // Summary by kV category (across all sigma thresholds)
    std::cout << "--- SUMMARY BY kV CATEGORY (across all sigma thresholds) ---" << std::endl;
    std::cout << "(Outlier detection uses data from row 22 onwards per file)" << std::endl;

    // Reorganize results by kV category
    std::map<int, std::vector<CategoryResult>> kV_grouped_results; // kV -> vector of results for different sigmas
    for (const auto& sigma_pair : all_results) {
        // int sigma = sigma_pair.first; // Not used directly here in grouping
        const std::vector<CategoryResult>& results = sigma_pair.second;
        for (const auto& result : results) {
             kV_grouped_results[result.kV].push_back(result);
        }
    }

    // Sort kV categories for consistent printing
    std::vector<int> sorted_kVs;
    for (const auto& kv_pair : kV_grouped_results) {
        sorted_kVs.push_back(kv_pair.first);
    }
    std::sort(sorted_kVs.begin(), sorted_kVs.end());

    for (int kV : sorted_kVs) {
        const std::vector<CategoryResult>& kV_results = kV_grouped_results[kV];
        std::cout << "kV Category " << kV << "kV:" << std::endl;
        // Check if there are results to avoid accessing empty vector
        if (!kV_results.empty()) {
             std::cout << "  Files in category: " << kV_results[0].total_files << std::endl; // Assumes all sigma results for a kV have same total_files
        } else {
             std::cout << "  Files in category: 0" << std::endl;
             continue; // Skip if no results
        }
        // For each sigma level, find the corresponding result for this kV
        for (int sigma = MIN_SIGMA_THRESHOLD; sigma <= MAX_SIGMA_THRESHOLD; sigma++) {
             auto sigma_it = all_results.find(sigma);
             if (sigma_it != all_results.end()) {
                 const std::vector<CategoryResult>& sigma_results = sigma_it->second;
                 // Find the result for this specific kV and sigma
                 auto res_it = std::find_if(sigma_results.begin(), sigma_results.end(),
                                            [kV](const CategoryResult& r) { return r.kV == kV; });
                 if (res_it != sigma_results.end()) {
                     const CategoryResult& result = *res_it;
                     // Print the sigma level along with the result details
                     std::cout << "    Sigma " << sigma << ": "
                               << result.files_with_outliers << "/" << result.total_files << " files with outliers "
                               << "(Efficiency: " << result.efficiency << " +/- " << result.efficiency_error << ")" << std::endl;
                 } else {
                      // Optional: Print if no data for this specific kV/sigma combination
                      // std::cout << "    Sigma " << sigma << ": No data" << std::endl;
                 }
             }
        }
        std::cout << std::endl;
    }

    // Grand totals (aggregated across all kV and sigma)
    std::cout << "--- GRAND TOTALS (aggregated across all kV and sigma) ---" << std::endl;
    std::cout << "(Outlier detection uses data from row 22 onwards per file)" << std::endl;
    int grand_total_files = 0;
    int grand_total_files_with_outliers = 0;
    // Iterate through all results to get grand totals
    for (const auto& sigma_pair : all_results) {
        const std::vector<CategoryResult>& results = sigma_pair.second;
        for (const auto& result : results) {
             grand_total_files += result.total_files;
             grand_total_files_with_outliers += result.files_with_outliers;
        }
    }
    double grand_overall_efficiency = (grand_total_files > 0) ?
                                        (static_cast<double>(grand_total_files_with_outliers) / static_cast<double>(grand_total_files)) :
                                        0.0;
    std::cout << "Grand total files analyzed (all kV, all sigma): " << grand_total_files << std::endl;
    std::cout << "Grand total files with outliers (all kV, all sigma): " << grand_total_files_with_outliers << std::endl;
    std::cout << "Grand Overall Efficiency (all kV, all sigma): " << grand_overall_efficiency << std::endl;
    if (grand_total_files > 0) {
        std::cout << "Grand Percentage of files with outliers (all kV, all sigma): "
                  << (100.0 * grand_total_files_with_outliers / grand_total_files) << "%" << std::endl;
    }
    std::cout << "\n" << std::string(100, '=') << std::endl;
}