//// File: threshold_crossing_analysis_with_date_plots.C
//// Description:
// This ROOT C++ macro scans the current directory for CSV files (.csv).
// It reads the amplitude data from Columns B (Trigger), C, D, and E (0-indexed), starting from row 22.
// For each file:
// 1. Calculates the mean and standard deviation of the Trigger data (Column B, from row 22 onwards).
// 2. Defines the threshold as: mean - n_sigma * standard_deviation.
// 3. Counts the number of times the Trigger signal crosses the defined threshold
//    from ABOVE to BELOW, ensuring that subsequent crossings within N rows are ignored.
// 4. Filters out files with fewer than 2 such threshold crossings.
// 5. For files with >= 2 crossings, it creates a single multi-panel PDF plot.
//    This PDF contains plots for Trigger (B), C, D, and E vs. Row Number.
//    The threshold line and valid crossing markers are shown on all plots.
//    The PDF filename is 'waveform_plots_<original_filename>.pdf'.
// 6. Categorizes files by kV level (3rd digit) and Date (5th digit) for summary plots.
//
// Output:
// - A summary for each kV level showing the count of files with specific
//   numbers of threshold crossings.
// - A list of the filenames of all files that had at least 2 threshold crossings.
// - A plot of kV (x-axis) vs. Number of files with >= 2 crossings (y-axis),
//   saved as 'kv_vs_file_count.pdf'.
// - A plot of kV (x-axis) vs. Mean row number of the 2nd crossing (y-axis) with 1-sigma error bars,
//   saved as 'kv_vs_mean_2nd_cross_row.pdf'.
// - A plot of kV (x-axis) vs. Average Standard Deviation of Amplitude per Row Index (y-axis),
//   saved as 'kv_vs_avg_stddev_per_row.pdf'.
// - Three new plots, grouped by DATE (5th digit):
//   a) Date vs. Number of files with >= 2 crossings.
//   b) Date vs. Mean row number of the 2nd crossing (with 1-sigma error bars).
//   c) Date vs. Average Standard Deviation of Amplitude per Row Index.
// - Individual multi-panel waveform plots for files with >= 2 crossings,
//   saved as 'waveform_plots_<original_filename>.pdf'.
//
// Dependencies:
// This script uses standard C++ file I/O for CSV parsing.
// It uses ROOT for statistics (TMath::Mean, TMath::RMS, TMath::StdDev), system functions,
// and plotting (TCanvas, TGraphErrors, TGraph, TAxis, TLine, TPaveText, TPad).
//
// How to Run in ROOT (Ubuntu):
// 1. Launch the ROOT interactive terminal:
//    root
// 2. Compile and execute this macro from the ROOT prompt:
//    .L threshold_crossing_analysis_with_date_plots.C+
//    threshold_crossing_analysis_with_date_plots()

// - Include necessary ROOT/Std C++ headers -
#include <TSystem.h>
#include <TString.h>
#include <TMath.h> // For TMath::Mean, TMath::RMS, TMath::StdDev
#include <TCanvas.h>
#include <TGraphErrors.h>
#include <TGraph.h> // For TGraph
#include <TAxis.h>
#include <TLine.h>
#include <TPaveText.h>
#include <TPad.h> // For TPad
#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <map>
#include <set>
#include <algorithm>
#include <cmath>
#include <limits>
#include <iomanip> // For std::setprecision
#include <numeric> // For std::accumulate

// - Configuration -
// Adjust this value to change the threshold sensitivity
const int N_SIGMA = 1;
const int DATA_START_ROW = 21; // 0-indexed, so 21 corresponds to the 22nd row
// Adjust this value to change the minimum separation between crossings (in rows)
const int MIN_CROSSING_SEPARATION = 10; // Example: 10 rows

// - Data Structures -
struct FileResult {
    std::string filename;
    int kV_category;    // e.g., 8, 9, ..., 14
    int date_category;  // New field for date
    int crossing_count; // Crossing from above to below with separation
    double mean;
    double std_dev; // Population std dev
    double threshold;
    int second_cross_row_index; // 0-indexed relative to the start of processed data
                                // To get actual CSV row: second_cross_row_index + DATA_START_ROW + 1
};

// - Global Variables -
std::vector<FileResult> valid_file_results;
// Map to store kV category -> map of crossing_count -> number of files
std::map<int, std::map<int, int>> kV_crossing_summary;
// Map to store kV category -> list of second crossing row indices (for mean/error calc)
std::map<int, std::vector<int>> kV_second_cross_indices;
// Map to store kV category -> list of standard deviations (for mean stddev per file)
std::map<int, std::vector<double>> kV_std_devs;

// Map to store Date category -> list of second crossing row indices (for mean/error calc)
std::map<int, std::vector<int>> date_second_cross_indices;
// Map to store Date category -> list of standard deviations (for mean stddev per file)
std::map<int, std::vector<double>> date_std_devs;

// - Function Declarations -
void threshold_crossing_analysis_with_date_plots();
// Helper function to read data from a single CSV file (Columns B-E) from a specific row
bool read_csv_data_from_row(const char* filename, std::vector<std::vector<double>>& all_data, int start_row, int num_columns = 4);
// Helper function to trim whitespace from strings
std::string trim(const std::string& str);
// Helper function to safely parse string to double
bool safe_string_to_double(const std::string& str, double& result);
// Helper function to extract numeric index from filename
int extract_numeric_index(const std::string& filename);
// Helper function to extract the kV category from the filename's numeric index
int extract_kV_category(int file_index);
// Helper function to extract the Date category from the filename's numeric index
int extract_date_category(int file_index);
// Helper function to count threshold crossings (above to below) with separation and find the row of the 2nd crossing
int count_threshold_crossings_above_to_below_with_separation_find_second(
    const std::vector<double>& trigger_data, double threshold, int min_separation, int& second_cross_row_index);
// New helper function to extract file number from filename
int extract_file_number(int file_index);
// New helper function to create individual multi-panel waveform plots
void create_multi_panel_waveform_plot(const std::string& filename,
                                      const std::vector<std::vector<double>>& all_data, // B, C, D, E
                                      double threshold,
                                      const std::vector<int>& crossing_indices);

// - Helper Function Implementations -

/**
 * @brief Reads data from specified columns (B-E by default) from a .csv file, starting from a specific row.
 * @param filename The name of the CSV file.
 * @param all_data Vector of vectors to store data for each requested column.
 *                 Index 0 = Column B, 1 = Column C, etc.
 * @param start_row The 0-indexed row number to start reading from (e.g., 21 for the 22nd row).
 * @param num_columns The number of columns to read (default 4 for B, C, D, E).
 * @return true if successful, false otherwise.
 */
bool read_csv_data_from_row(const char* filename, std::vector<std::vector<double>>& all_data, int start_row, int num_columns) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cout << " -> Error: Could not open file " << filename << std::endl;
        return false;
    }

    // Resize all_data to hold vectors for the requested columns
    all_data.resize(num_columns);

    std::string line;
    int current_line_num = 0;
    int valid_rows = 0;
    int invalid_rows = 0;

    // Clear data vectors
    for (auto& col_data : all_data) {
        col_data.clear();
    }

    while (std::getline(file, line)) {
        // Skip lines until we reach the start row
        if (current_line_num < start_row) {
            current_line_num++;
            continue;
        }
        if (trim(line).empty()) {
            current_line_num++;
            continue; // Skip empty lines
        }

        std::stringstream ss(line);
        std::string segment;
        // Parse up to the last requested column (B is index 1, so last is 1 + num_columns - 1)
        int col_count = 0;
        std::vector<double> row_values(num_columns, std::numeric_limits<double>::quiet_NaN());

        while(col_count < (1 + num_columns) && std::getline(ss, segment, ',')) {
            if (col_count >= 1 && col_count < (1 + num_columns)) { // Columns B (1) to E (4)
                int data_index = col_count - 1; // Map column B->0, C->1, etc.
                if (!safe_string_to_double(trim(segment), row_values[data_index])) {
                    row_values[data_index] = std::numeric_limits<double>::quiet_NaN();
                }
            }
            col_count++;
        }

        // Check if at least one value in the row is valid
        bool has_valid_data = false;
        for (int i = 0; i < num_columns; ++i) {
            if (!std::isnan(row_values[i]) && std::isfinite(row_values[i])) {
                has_valid_data = true;
                break;
            }
        }

        if (has_valid_data) {
            for (int i = 0; i < num_columns; ++i) {
                all_data[i].push_back(row_values[i]);
            }
            valid_rows++;
        } else {
            invalid_rows++;
        }
        current_line_num++;
    }
    file.close();

    if (valid_rows == 0) {
        std::cout << " -> Warning: No valid data rows found in file " << filename << " for columns B-E starting from row " << (start_row + 1) << std::endl;
        return false; // Return false if no valid data was found
    } else {
        // std::cout << " -> Read " << valid_rows << " valid data rows (from row " << (start_row + 1) << "+) for columns B-E." << std::endl; // Printed in main loop
        return true;
    }
}

/**
 * @brief Trims whitespace from the beginning and end of a string.
 * @param str The string to trim.
 * @return The trimmed string.
 */
std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r");
    if (first == std::string::npos) {
        return "";
    }
    size_t last = str.find_last_not_of(" \t\r");
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
        // stod can handle exponential notation like -1.70E-7
        size_t processed = 0;
        result = std::stod(str, &processed);
        // Check if the entire string was processed
        if (processed != str.length()) {
            return false;
        }
        // Check if the result is valid
        return std::isfinite(result) && !std::isnan(result);
    } catch (const std::invalid_argument&) {
        return false;
    } catch (const std::out_of_range&) {
        return false;
    }
}

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
    if (name_without_ext.empty() || name_without_ext.find_first_not_of("0123456789") != std::string::npos) {
        return -1; // Not a numeric filename
    }

    try {
        return std::stoi(name_without_ext);
    } catch (const std::exception&) {
        return -1; // Conversion failed
    }
}

/**
 * @brief Extracts the kV category from the filename's numeric index.
 *        The 3rd digit from the right determines the kV.
 *        0->8kV, 1->9kV, ..., 6->14kV. Default is 8kV.
 * @param file_index The numeric index extracted from the filename.
 * @return The kV category (e.g., 8, 9, ..., 14).
 */
int extract_kV_category(int file_index) {
    // Convert to string to easily access digits
    std::string index_str = std::to_string(file_index);
    int len = index_str.length();

    // Get the 3rd digit from the right (100s place)
    int third_digit_from_right = 0; // Default
    if (len >= 3) {
        // e.g., index_str = "12345", len=5. We want index 5-3 = 2, char '3'
        third_digit_from_right = index_str[len - 3] - '0'; // Convert char to int
    }

    // Map 0->8kV, 1->9kV, ..., 6->14kV
    return 8 + third_digit_from_right;
}

/**
 * @brief Extracts the Date category from the filename's numeric index.
 *        The 5th digit from the right determines the date.
 *        0->7/16, 1->7/17, 2->7/18, 3->7/21. Default is 7/16.
 * @param file_index The numeric index extracted from the filename.
 * @return The Date category (e.g., 716, 717, 718, 721).
 */
int extract_date_category(int file_index) {
    // Convert to string to easily access digits
    std::string index_str = std::to_string(file_index);
    int len = index_str.length();

    // Get the 5th digit from the right
    int fifth_digit_from_right = 0; // Default to 0 -> 7/16
    if (len >= 5) {
        // e.g., index_str = "1234567", len=7. We want index 7-5 = 2, char '3'
        fifth_digit_from_right = index_str[len - 5] - '0'; // Convert char to int
    }

    // Map 0->7/16, 1->7/17, 2->7/18, 3->7/21
    switch (fifth_digit_from_right) {
        case 1: return 717;
        case 2: return 718;
        case 3: return 721;
        default: return 716; // Case 0 or any other value
    }
}

/**
 * @brief Extracts the file number from the filename's numeric index.
 *        The last two digits of the filename determine the file number.
 * @param file_index The numeric index extracted from the filename.
 * @return The file number (00-99).
 */
int extract_file_number(int file_index) {
    // Get the last two digits
    return file_index % 100;
}


/**
 * @brief Counts the number of times the signal crosses the threshold from above to below,
 *        enforcing a minimum separation between crossings.
 *        Also finds the row index of the 2nd crossing.
 * @param data The vector of data points (typically Trigger/Column B).
 * @param threshold The threshold value.
 * @param min_separation The minimum number of rows that must separate two crossings.
 * @param second_cross_row_index Output: The 0-indexed row of the 2nd crossing (relative to data start).
 *                              Set to -1 if not found.
 * @return The total number of crossings (from above to below) with enforced separation.
 */
int count_threshold_crossings_above_to_below_with_separation_find_second(
    const std::vector<double>& data, double threshold, int min_separation, int& second_cross_row_index) {

    // Initialize output
    second_cross_row_index = -1;
    if (data.size() < 2) return 0; // Need at least two points to have a potential crossing

    int crossings = 0;
    int last_crossing_index = -1; // Index of the last recorded crossing

    // Initialize state based on the first point
    bool was_above = (data[0] > threshold);

    for (size_t i = 1; i < data.size(); ++i) {
        bool is_below_or_at = (data[i] <= threshold); // Crossing is when it goes from > to <=

        // A potential crossing occurs if we were above and are now at or below the threshold
        if (was_above && is_below_or_at) {
            // Check if this crossing is far enough from the last one (or if it's the first)
            if (last_crossing_index == -1 || (static_cast<int>(i) - last_crossing_index) >= min_separation) {
                crossings++;
                last_crossing_index = i; // Update the index of the last recorded crossing

                if (crossings == 2) {
                    second_cross_row_index = i; // Store index of second crossing
                    // Note: We don't break here to continue counting all valid crossings
                }
            }
            // If not far enough, this crossing is ignored (not counted)
        }

        // Update state for next iteration
        was_above = (data[i] > threshold); // Next iteration checks if *this* point was above
    }

    return crossings;
}


/**
 * @brief Creates a multi-panel waveform plot PDF for a file with >= 2 crossings.
 * @param filename The name of the CSV file.
 * @param all_data Vector containing data for Columns B, C, D, E.
 * @param threshold The calculated threshold value (from Column B).
 * @param crossing_indices Vector of indices where valid crossings occurred (relative to data start).
 */
void create_multi_panel_waveform_plot(const std::string& filename,
                                      const std::vector<std::vector<double>>& all_data, // B, C, D, E
                                      double threshold,
                                      const std::vector<int>& crossing_indices) {
    // Ensure we have data for at least Column B
    if (all_data.empty() || all_data[0].empty()) {
        std::cout << " -> Warning: No data provided for plotting file " << filename << std::endl;
        return;
    }
    const std::vector<double>& trigger_data = all_data[0]; // Column B

    int file_index = extract_numeric_index(filename);
    if (file_index < 0) return; // Should not happen for valid files

    int date_cat = extract_date_category(file_index);
    int kv_cat = extract_kV_category(file_index);
    int file_num = extract_file_number(file_index);

    const int num_plots = 4; // B, C, D, E
    const char* plot_titles[num_plots] = {"Trigger (Column B)", "Column C", "Column D", "Column E"};
    const int line_colors[num_plots] = {kBlue, kRed, kGreen+2, kMagenta};

    // Create a single canvas for all plots
    TCanvas* canvas = new TCanvas(("c_multi_waveform_" + filename).c_str(), ("Waveforms: " + filename).c_str(), 1200, 1000);
    canvas->Divide(2, 2); // 2 columns, 2 rows

    // Create graphs for each column
    std::vector<TGraph*> graphs(num_plots, nullptr);
    for (int p = 0; p < num_plots; ++p) {
        if (p < static_cast<int>(all_data.size()) && !all_data[p].empty()) {
            graphs[p] = new TGraph(all_data[p].size());
            for (size_t i = 0; i < all_data[p].size(); ++i) {
                graphs[p]->SetPoint(i, DATA_START_ROW + i + 1, all_data[p][i]); // Use actual CSV row numbers on x-axis
            }
        }
    }

    // Draw each plot
    for (int p = 0; p < num_plots; ++p) {
        canvas->cd(p + 1); // Switch to pad p+1 (1-indexed)
        gPad->SetGrid();

        if (graphs[p]) {
            graphs[p]->SetTitle(plot_titles[p]);
            graphs[p]->GetXaxis()->SetTitle("CSV Row Number");
            graphs[p]->GetYaxis()->SetTitle("Amplitude");
            graphs[p]->SetLineWidth(2);
            graphs[p]->SetLineColor(line_colors[p]);
            graphs[p]->SetMarkerStyle(20); // Small dot marker
            graphs[p]->SetMarkerSize(0.4);
            graphs[p]->SetMarkerColor(line_colors[p]);
            graphs[p]->Draw("ALP"); // Axis, Line, Points

            // Draw threshold line (only relevant for Trigger B, but shown for context on all)
            double min_x = DATA_START_ROW + 1;
            double max_x = DATA_START_ROW + trigger_data.size(); // Use trigger data size for x-axis range
            TLine* threshold_line = new TLine(min_x, threshold, max_x, threshold);
            threshold_line->SetLineColor(kBlack);
            threshold_line->SetLineStyle(2); // Dashed
            threshold_line->SetLineWidth(2);
            threshold_line->Draw("same");

            // Draw crossing markers on all plots
            for (int cross_index : crossing_indices) {
                if (cross_index >= 0 && cross_index < static_cast<int>(trigger_data.size())) {
                    double x_pos = DATA_START_ROW + cross_index + 1;
                    TLine* marker = new TLine(x_pos, gPad->GetUymin(), x_pos, gPad->GetUymax());
                    marker->SetLineColor(kOrange+7);
                    marker->SetLineWidth(2);
                    marker->Draw("same");
                }
            }
        } else {
            // Handle case where data for a column is missing
            TPaveText* pt = new TPaveText(0.1, 0.1, 0.9, 0.9, "NDC");
            pt->AddText("No Data Available");
            pt->Draw();
        }
    }

    // Add overall title with date, kV, file number
    canvas->cd(); // Go back to canvas, not a specific pad
    TPaveText* title = new TPaveText(0.1, 0.96, 0.9, 0.99, "NDC");
    title->SetFillColor(kWhite);
    title->SetBorderSize(0);
    title->SetTextAlign(12); // Left aligned, center vertical
    title->AddText(Form("File: %s | Date: 2025-07/%02d | KV: %dkV | File #: %02d", filename.c_str(), date_cat % 100, kv_cat, file_num));
    title->Draw();

    // Save plot to a single PDF file
    std::string pdf_name = "waveform_plots_" + filename + ".pdf";
    // Remove .csv extension if present in the filename part
    size_t dot_pos = pdf_name.find_last_of('.');
    if (dot_pos != std::string::npos && pdf_name.substr(dot_pos) == ".csv.pdf") {
        pdf_name = pdf_name.substr(0, dot_pos) + ".pdf";
    }
    canvas->SaveAs(pdf_name.c_str());

    std::cout << "   -> Multi-panel plot saved to '" << pdf_name << "'" << std::endl;

    // Cleanup
    for (auto* graph : graphs) {
        delete graph; // TGraph destructor handles points
    }
    delete canvas;
    delete title;
    // Note: TLine objects (threshold_line, marker) are owned by the pad/canvas and will be deleted with it
}


// - Main Processing Function -
void threshold_crossing_analysis_with_date_plots() {
    std::cout << "=============================================================" << std::endl;
    std::cout << "Starting Threshold Crossing Analysis (With Date Plots)..." << std::endl;
    std::cout << "=============================================================" << std::endl;
    std::cout << "Reading data from Columns B-E starting from row " << (DATA_START_ROW + 1) << "." << std::endl;
    std::cout << "Threshold (for crossings) defined from Column B: Mean - " << N_SIGMA << " * Standard Deviation." << std::endl;
    std::cout << "Counting crossings: Transitions from ABOVE threshold to BELOW threshold (Column B)." << std::endl;
    std::cout << "Minimum separation between crossings: " << MIN_CROSSING_SEPARATION << " rows." << std::endl;
    std::cout << "Files must have at least 2 crossings to be considered." << std::endl;
    std::cout << "kV categories determined by the 3rd digit of the numeric filename (0->8kV, 1->9kV, ..., 6->14kV)." << std::endl;
    std::cout << "Date categories determined by the 5th digit of the numeric filename (0->7/16, 1->7/17, 2->7/18, 3->7/21)." << std::endl;
    std::cout << "=============================================================" << std::endl;

    // Clear global variables
    valid_file_results.clear();
    kV_crossing_summary.clear();
    kV_second_cross_indices.clear();
    kV_std_devs.clear();
    date_second_cross_indices.clear();
    date_std_devs.clear();

    // - PASS 1: Read all CSV files with numeric names, collect data -
    std::cout << "=== PASS 1: Reading CSV files with numeric names (from row " << (DATA_START_ROW + 1) << ") ===" << std::endl;
    void* dir_handle = gSystem->OpenDirectory(".");
    if (!dir_handle) {
        std::cerr << "Error: Could not open current directory." << std::endl;
        return;
    }

    const char* entry;
    int total_csv_files = 0;
    int files_processed_successfully = 0;
    int files_with_enough_crossings = 0;

    // Use a map to store filename -> all_data for efficient lookup in PASS 2
    // all_data[0] = Column B (Trigger), [1] = C, [2] = D, [3] = E
    std::map<std::string, std::vector<std::vector<double>>> filename_to_all_data_map;

    while ((entry = gSystem->GetDirEntry(dir_handle))) {
        TString filename(entry);
        if (filename.EndsWith(".csv")) {
            total_csv_files++;
            int file_index = extract_numeric_index(filename.Data());
            if (file_index >= 0) { // Only process files with numeric names
                std::cout << "Reading data from CSV file: " << filename << " (index: " << file_index << ")" << std::endl;
                std::vector<std::vector<double>> all_data; // Will hold B, C, D, E
                if (read_csv_data_from_row(filename.Data(), all_data, DATA_START_ROW, 4)) {
                    files_processed_successfully++;
                    filename_to_all_data_map[filename.Data()] = std::move(all_data); // Move for efficiency
                }
            }
        }
    }
    gSystem->FreeDirectory(dir_handle);
    std::cout << "=============================================================" << std::endl;
    std::cout << "Total .csv files found: " << total_csv_files << std::endl;
    std::cout << "Files processed successfully: " << files_processed_successfully << std::endl;
    std::cout << "=============================================================" << std::endl;

    // - PASS 2: Analyze Trigger data (Column B) for threshold crossings -
    std::cout << "=== PASS 2: Analyzing Trigger data (Column B) for threshold crossings ===" << std::endl;
    for (const auto& file_data_pair : filename_to_all_data_map) {
        const std::string& filename = file_data_pair.first;
        const std::vector<std::vector<double>>& all_data = file_data_pair.second;

        // Check if we have data for Column B (index 0)
        if (all_data.empty() || all_data[0].empty()) {
            std::cout << "Skipping " << filename << " due to missing Column B data." << std::endl;
            continue;
        }
        const std::vector<double>& trigger_data = all_data[0]; // Column B

        // - Calculate Statistics for Column B -
        double mean = TMath::Mean(trigger_data.size(), trigger_data.data());
        // Note: TMath::RMS calculates population standard deviation
        double std_dev = TMath::RMS(trigger_data.size(), trigger_data.data());

        // - Define Threshold (Lower Bound) -
        double threshold = mean - N_SIGMA * std_dev;

        // - Count Threshold Crossings (Above to Below with Separation) and Find 2nd Crossing Row -
        int second_cross_row_index = -1; // Initialize to -1 (not found)
        int crossing_count = count_threshold_crossings_above_to_below_with_separation_find_second(trigger_data, threshold, MIN_CROSSING_SEPARATION, second_cross_row_index);

        std::cout << " -> File: " << filename << std::endl;
        std::cout << "   Mean (B): " << std::fixed << std::setprecision(6) << mean << ", Std Dev (B): " << std_dev << std::endl;
        std::cout << "   Threshold (Mean - " << N_SIGMA << "*Sigma): " << threshold << std::endl;
        std::cout << "   Crossings (B, Above to Below with " << MIN_CROSSING_SEPARATION << "+ row separation): " << crossing_count << std::endl;

        // - Filter and Store Results -
        if (crossing_count >= 2) {
            files_with_enough_crossings++;
            std::cout << "   -> Accepted (>= 2 crossings). 2nd crossing at relative row index: " << second_cross_row_index << std::endl;

            // Extract categories
            int file_index = extract_numeric_index(filename);
            int kV_category = extract_kV_category(file_index);
            int date_category = extract_date_category(file_index); // New

            // Store detailed result
            FileResult result;
            result.filename = filename;
            result.kV_category = kV_category;
            result.date_category = date_category; // New
            result.crossing_count = crossing_count;
            result.mean = mean;
            result.std_dev = std_dev;
            result.threshold = threshold;
            result.second_cross_row_index = second_cross_row_index;
            valid_file_results.push_back(result);

            // Update summary maps
            kV_crossing_summary[kV_category][crossing_count]++;
            if (second_cross_row_index != -1) {
                kV_second_cross_indices[kV_category].push_back(second_cross_row_index);
            }
            kV_std_devs[kV_category].push_back(std_dev);

            // Update date maps
            if (second_cross_row_index != -1) {
                date_second_cross_indices[date_category].push_back(second_cross_row_index);
            }
            date_std_devs[date_category].push_back(std_dev);

            // --- NEW: Create multi-panel waveform plot for files with >= 2 crossings ---
            // Find all valid crossing indices for plotting markers
            std::vector<int> all_crossing_indices;
            int dummy_second_index = -1; // Not used here
            count_threshold_crossings_above_to_below_with_separation_find_second(trigger_data, threshold, MIN_CROSSING_SEPARATION, dummy_second_index);
            // Re-implement simple crossing detection to collect indices
            bool was_above_plot = (trigger_data[0] > threshold);
            int last_crossing_index_plot = -1;
            for (size_t i = 1; i < trigger_data.size(); ++i) {
                bool is_below_or_at_plot = (trigger_data[i] <= threshold);
                if (was_above_plot && is_below_or_at_plot) {
                     if (last_crossing_index_plot == -1 || (static_cast<int>(i) - last_crossing_index_plot) >= MIN_CROSSING_SEPARATION) {
                         all_crossing_indices.push_back(i);
                         last_crossing_index_plot = i;
                     }
                }
                was_above_plot = (trigger_data[i] > threshold);
            }
            create_multi_panel_waveform_plot(filename, all_data, threshold, all_crossing_indices);
            // --- END NEW ---

        } else {
            std::cout << "   -> Rejected (< 2 crossings)." << std::endl;
        }
    }
    std::cout << "=============================================================" << std::endl;
    std::cout << "Files with >= 2 threshold crossings: " << files_with_enough_crossings << std::endl;
    std::cout << "=============================================================" << std::endl;

    // - PASS 3: Generate Reports and Plots -
    std::cout << "=== PASS 3: Generating Reports and Plots ===" << std::endl;

    if (valid_file_results.empty()) {
        std::cout << "No files met the threshold crossing criteria." << std::endl;
    } else {
        // Sort filenames alphabetically for consistent output
        std::sort(valid_file_results.begin(), valid_file_results.end(),
                  [](const FileResult& a, const FileResult& b) { return a.filename < b.filename; });

        std::cout << std::fixed << std::setprecision(6);

        // --- Report 1: List of all files with >= 2 crossings ---
        std::cout << "\n--- Report 1: Files with >= 2 Threshold Crossings ---" << std::endl;
        for (const auto& result : valid_file_results) {
            std::cout << "File: " << result.filename << std::endl;
            std::cout << "  kV Category: ";
            if (result.kV_category >= 8 && result.kV_category <= 14) {
                std::cout << result.kV_category << "kV";
            } else {
                std::cout << "Unknown (" << result.kV_category << ")";
            }
            std::cout << ", Date Category: " << result.date_category << std::endl; // New
            std::cout << "  Crossings: " << result.crossing_count
                      << ", Mean (B): " << result.mean << ", Std Dev (B): " << result.std_dev
                      << ", Threshold: " << result.threshold << std::endl;
            std::cout << "  2nd Crossing CSV Row: " << (result.second_cross_row_index + DATA_START_ROW + 1) << std::endl;
        }

        // --- Report 2: kV Crossing Summary ---
        std::cout << "\n--- Report 2: kV Category vs. Crossing Count Summary ---" << std::endl;
        for (const auto& kv_pair : kV_crossing_summary) {
            int kV_cat = kv_pair.first;
            const std::map<int, int>& crossings_map = kv_pair.second;
            std::cout << "kV " << kV_cat << "kV: ";
            for (const auto& cross_pair : crossings_map) {
                int count_of_crossings = cross_pair.first;
                int num_files = cross_pair.second;
                std::cout << num_files << " file(s) with " << count_of_crossings << " crosses, ";
            }
            std::cout << std::endl;
        }

        // --- Plot 1: kV vs File Count ---
        std::cout << "\n--- Plot 1: Creating kV vs File Count ---" << std::endl;
        std::vector<double> kV_values_count, file_counts;
        for (const auto& kv_pair : kV_crossing_summary) {
            int kV_cat = kv_pair.first;
            const std::map<int, int>& crossings_map = kv_pair.second;
            int total_files_for_kV = 0;
            for (const auto& cross_pair : crossings_map) {
                total_files_for_kV += cross_pair.second;
            }
            kV_values_count.push_back(kV_cat);
            file_counts.push_back(total_files_for_kV);
            std::cout << "kV " << kV_cat << "kV: " << total_files_for_kV << " files with >= 2 crosses." << std::endl;
        }

        if (kV_values_count.empty() || file_counts.empty()) {
            std::cout << "No data available to plot kV vs File Count." << std::endl;
        } else {
            TGraph* graph_count = new TGraph(kV_values_count.size(), kV_values_count.data(), file_counts.data());
            graph_count->SetTitle("Number of Files vs kV; kV Category (kV); Number of Files (>= 2 Crosses)");
            graph_count->SetMarkerStyle(20);
            graph_count->SetMarkerSize(1.5);
            graph_count->SetMarkerColor(kBlue);
            graph_count->SetLineColor(kBlue);
            graph_count->SetLineWidth(2);

            // Create Canvas
            TCanvas* canvas_count = new TCanvas("c_kv_vs_file_count", "kV vs File Count", 800, 600);
            canvas_count->SetGrid();
            graph_count->Draw("APL"); // Axis, Points, Line

            // Improve axes
            graph_count->GetXaxis()->SetLimits(7.5, 14.5); // Slightly wider than 8-14
            graph_count->GetYaxis()->SetRangeUser(0, *std::max_element(file_counts.begin(), file_counts.end()) * 1.1 + 1);

            // Save to PDF
            TString plot_filename_count = "kv_vs_file_count.pdf";
            canvas_count->SaveAs(plot_filename_count);
            std::cout << "Plot saved to '" << plot_filename_count << "'." << std::endl;

            // Clean up
            delete graph_count;
            delete canvas_count;
        }

        // --- Plot 2: kV vs Mean 2nd Crossing Row ---
        std::cout << "\n--- Plot 2: Creating kV vs Mean 2nd Crossing Row ---" << std::endl;
        std::vector<double> kV_values_row, mean_rows, mean_row_errors;
        for (const auto& kv_pair : kV_second_cross_indices) {
            int kV_cat = kv_pair.first;
            const std::vector<int>& indices = kv_pair.second;
            if (!indices.empty()) {
                double mean_index = TMath::Mean(indices.size(), indices.data());
                // Calculate standard deviation of the sample
                double std_dev_index = TMath::StdDev(indices.size(), indices.data());
                // Calculate standard error of the mean (SEM)
                double sem = (indices.size() > 1) ? (std_dev_index / std::sqrt(static_cast<double>(indices.size()))) : 0.0;

                kV_values_row.push_back(kV_cat);
                mean_rows.push_back(mean_index);
                mean_row_errors.push_back(sem); // Use SEM for y-error bars

                std::cout << "kV " << kV_cat << "kV: Mean 2nd cross row index = "
                          << mean_index << " +/- " << sem << " (N=" << indices.size() << ")" << std::endl;
            }
        }

        if (kV_values_row.empty() || mean_rows.empty()) {
            std::cout << "No data available to plot kV vs Mean 2nd Crossing Row." << std::endl;
        } else {
            TGraphErrors* graph_row = new TGraphErrors(kV_values_row.size(), kV_values_row.data(), mean_rows.data(),
                                                       nullptr, mean_row_errors.data()); // nullptr for x-errors
            graph_row->SetTitle("Mean 2nd Crossing Row vs kV; kV Category (kV); Mean Row Index of 2nd Crossing");
            graph_row->SetMarkerStyle(21); // Full square
            graph_row->SetMarkerSize(1.2);
            graph_row->SetMarkerColor(kRed);
            graph_row->SetLineColor(kRed);
            graph_row->SetLineWidth(2);
            graph_row->SetFillColor(kRed-10); // Light red fill for error bars

            // Create Canvas
            TCanvas* canvas_row = new TCanvas("c_kv_vs_mean_row", "kV vs Mean 2nd Crossing Row", 800, 600);
            canvas_row->SetGrid();
            graph_row->Draw("AP"); // Axis, Points (error bars drawn automatically)

            // Improve axes
            graph_row->GetXaxis()->SetLimits(7.5, 14.5);
            double max_y = *std::max_element(mean_rows.begin(), mean_rows.end());
            double min_y = *std::min_element(mean_rows.begin(), mean_rows.end());
            double range_y = max_y - min_y;
            graph_row->GetYaxis()->SetRangeUser((min_y - range_y * 0.1), (max_y + range_y * 0.1));

            // Save to PDF
            TString plot_filename_row = "kv_vs_mean_2nd_cross_row.pdf";
            canvas_row->SaveAs(plot_filename_row);
            std::cout << "Plot saved to '" << plot_filename_row << "'." << std::endl;

            // Clean up
            delete graph_row;
            delete canvas_row;
        }


        // --- Plot 3: kV vs Average Standard Deviation ---
        std::cout << "\n--- Plot 3: Creating kV vs Average Std Dev ---" << std::endl;
        std::vector<double> kV_values_stddev, avg_stddevs;
        for (const auto& kv_pair : kV_std_devs) {
            int kV_cat = kv_pair.first;
            const std::vector<double>& stddevs = kv_pair.second;
            if (!stddevs.empty()) {
                double avg_stddev = TMath::Mean(stddevs.size(), stddevs.data());
                kV_values_stddev.push_back(kV_cat);
                avg_stddevs.push_back(avg_stddev);
                std::cout << "kV " << kV_cat << "kV: Average Std Dev = " << avg_stddev << " (N=" << stddevs.size() << ")" << std::endl;
            }
        }

        if (kV_values_stddev.empty() || avg_stddevs.empty()) {
            std::cout << "No data available to plot kV vs Average Std Dev." << std::endl;
        } else {
             TGraph* graph_stddev = new TGraph(kV_values_stddev.size(), kV_values_stddev.data(), avg_stddevs.data());

             graph_stddev->SetTitle("Average Std Dev vs kV; kV Category (kV); Average Std Dev of Amplitude (Column B)");
             graph_stddev->SetMarkerStyle(22); // Triangle up
             graph_stddev->SetMarkerSize(1.2);
             graph_stddev->SetMarkerColor(kGreen+2);
             graph_stddev->SetLineColor(kGreen+2);
             graph_stddev->SetLineWidth(2);

             // Create Canvas
             TCanvas* canvas_stddev = new TCanvas("c_kv_vs_avg_stddev", "kV vs Avg Std Dev", 800, 600);
             canvas_stddev->SetGrid();
             graph_stddev->Draw("AP");

             // Improve axes
             graph_stddev->GetXaxis()->SetLimits(7.5, 14.5);
             double max_y_stddev = *std::max_element(avg_stddevs.begin(), avg_stddevs.end());
             double min_y_stddev = *std::min_element(avg_stddevs.begin(), avg_stddevs.end());
             double range_y_stddev = max_y_stddev - min_y_stddev;
             if (range_y_stddev > 0) {
                 graph_stddev->GetYaxis()->SetRangeUser((min_y_stddev - range_y_stddev * 0.05), (max_y_stddev + range_y_stddev * 0.05));
             } else {
                 graph_stddev->GetYaxis()->SetRangeUser(min_y_stddev * 0.95, max_y_stddev * 1.05);
             }


             // Save to PDF
             TString plot_filename_stddev = "kv_vs_avg_stddev_per_row.pdf";
             canvas_stddev->SaveAs(plot_filename_stddev);
             std::cout << "Plot saved to '" << plot_filename_stddev << "'." << std::endl;

             // Clean up
             delete graph_stddev;
             delete canvas_stddev;
        }


        // --- NEW PLOTS: Grouped by DATE ---

        // --- Plot 4: Date vs File Count ---
        std::cout << "\n--- Plot 4: Creating Date vs File Count ---" << std::endl;
        // We need to iterate through date categories in a sorted way for consistent plotting
        std::set<int> date_categories_found;
        for (const auto& date_pair : date_second_cross_indices) {
             date_categories_found.insert(date_pair.first);
        }

        std::vector<double> date_values_count, date_file_counts;
        std::map<int, std::string> date_labels = {{716, "7/16"}, {717, "7/17"}, {718, "7/18"}, {721, "7/21"}};

        for (int date_cat : date_categories_found) {
            // Count files for this date category
            auto it_indices = date_second_cross_indices.find(date_cat);
            auto it_stddevs = date_std_devs.find(date_cat);
            int num_files_indices = (it_indices != date_second_cross_indices.end()) ? it_indices->second.size() : 0;
            int num_files_stddevs = (it_stddevs != date_std_devs.end()) ? it_stddevs->second.size() : 0;
            // The number of files should be consistent between indices and stddevs maps
            int total_files_for_date = std::max(num_files_indices, num_files_stddevs);

            date_values_count.push_back(date_cat); // Use the numeric date category (716, 717, etc.) for x-axis
            date_file_counts.push_back(total_files_for_date);
            std::cout << "Date " << date_labels[date_cat] << " (Category " << date_cat << "): " << total_files_for_date << " files with >= 2 crosses." << std::endl;
        }

        if (date_values_count.empty() || date_file_counts.empty()) {
            std::cout << "No data available to plot Date vs File Count." << std::endl;
        } else {
            TGraph* graph_date_count = new TGraph(date_values_count.size(), date_values_count.data(), date_file_counts.data());
            graph_date_count->SetTitle("Number of Files vs Date; Date Category; Number of Files (>= 2 Crosses)");
            graph_date_count->SetMarkerStyle(20);
            graph_date_count->SetMarkerSize(1.5);
            graph_date_count->SetMarkerColor(kMagenta);
            graph_date_count->SetLineColor(kMagenta);
            graph_date_count->SetLineWidth(2);

            // Create Canvas
            TCanvas* canvas_date_count = new TCanvas("c_date_vs_file_count", "Date vs File Count", 800, 600);
            canvas_date_count->SetGrid();
            graph_date_count->Draw("APL");

            // Improve axes - Use custom labels for x-axis
            graph_date_count->GetXaxis()->SetLimits(*date_values_count.begin() - 1, *date_values_count.rbegin() + 1);
            // Set custom labels
            graph_date_count->GetXaxis()->SetNdivisions(date_values_count.size()); // Reduce automatic divisions
            for (size_t i = 0; i < date_values_count.size(); ++i) {
                 graph_date_count->GetXaxis()->SetBinLabel(i+1, date_labels[date_values_count[i]].c_str());
            }
            graph_date_count->GetYaxis()->SetRangeUser(0, *std::max_element(date_file_counts.begin(), date_file_counts.end()) * 1.1 + 1);
            graph_date_count->GetXaxis()->SetTitle("Date");
            graph_date_count->GetXaxis()->CenterTitle();

            // Save to PDF
            TString plot_filename_date_count = "date_vs_file_count.pdf";
            canvas_date_count->SaveAs(plot_filename_date_count);
            std::cout << "Plot saved to '" << plot_filename_date_count << "'." << std::endl;

            // Clean up
            delete graph_date_count;
            delete canvas_date_count;
        }


        // --- Plot 5: Date vs Mean 2nd Crossing Row ---
        std::cout << "\n--- Plot 5: Creating Date vs Mean 2nd Crossing Row ---" << std::endl;
        std::vector<double> date_values_row, date_mean_rows, date_mean_row_errors;

        for (int date_cat : date_categories_found) {
            auto it = date_second_cross_indices.find(date_cat);
            if (it != date_second_cross_indices.end() && !it->second.empty()) {
                const std::vector<int>& indices = it->second;
                double mean_index = TMath::Mean(indices.size(), indices.data());
                double std_dev_index = TMath::StdDev(indices.size(), indices.data());
                double sem = (indices.size() > 1) ? (std_dev_index / std::sqrt(static_cast<double>(indices.size()))) : 0.0;

                date_values_row.push_back(date_cat);
                date_mean_rows.push_back(mean_index);
                date_mean_row_errors.push_back(sem);

                std::cout << "Date " << date_labels[date_cat] << " (Category " << date_cat << "): Mean 2nd cross row index = "
                          << mean_index << " +/- " << sem << " (N=" << indices.size() << ")" << std::endl;
            }
        }

        if (date_values_row.empty() || date_mean_rows.empty()) {
            std::cout << "No data available to plot Date vs Mean 2nd Crossing Row." << std::endl;
        } else {
            TGraphErrors* graph_date_row = new TGraphErrors(date_values_row.size(), date_values_row.data(), date_mean_rows.data(),
                                                       nullptr, date_mean_row_errors.data());
            graph_date_row->SetTitle("Mean 2nd Crossing Row vs Date; Date; Mean Row Index of 2nd Crossing");
            graph_date_row->SetMarkerStyle(21);
            graph_date_row->SetMarkerSize(1.2);
            graph_date_row->SetMarkerColor(kOrange+7);
            graph_date_row->SetLineColor(kOrange+7);
            graph_date_row->SetLineWidth(2);
            graph_date_row->SetFillColor(kOrange-3);

            // Create Canvas
            TCanvas* canvas_date_row = new TCanvas("c_date_vs_mean_row", "Date vs Mean 2nd Crossing Row", 800, 600);
            canvas_date_row->SetGrid();
            graph_date_row->Draw("AP");

            // Improve axes
            graph_date_row->GetXaxis()->SetLimits(*date_values_row.begin() - 1, *date_values_row.rbegin() + 1);
            for (size_t i = 0; i < date_values_row.size(); ++i) {
                 graph_date_row->GetXaxis()->SetBinLabel(i+1, date_labels[date_values_row[i]].c_str());
            }
            double max_y_date = *std::max_element(date_mean_rows.begin(), date_mean_rows.end());
            double min_y_date = *std::min_element(date_mean_rows.begin(), date_mean_rows.end());
            double range_y_date = max_y_date - min_y_date;
            graph_date_row->GetYaxis()->SetRangeUser((min_y_date - range_y_date * 0.1), (max_y_date + range_y_date * 0.1));
            graph_date_row->GetXaxis()->SetTitle("Date");
            graph_date_row->GetXaxis()->CenterTitle();

            // Save to PDF
            TString plot_filename_date_row = "date_vs_mean_2nd_cross_row.pdf";
            canvas_date_row->SaveAs(plot_filename_date_row);
            std::cout << "Plot saved to '" << plot_filename_date_row << "'." << std::endl;

            // Clean up
            delete graph_date_row;
            delete canvas_date_row;
        }


        // --- Plot 6: Date vs Average Standard Deviation ---
        std::cout << "\n--- Plot 6: Creating Date vs Average Std Dev ---" << std::endl;
        std::vector<double> date_values_stddev, date_avg_stddevs;

        for (int date_cat : date_categories_found) {
            auto it = date_std_devs.find(date_cat);
            if (it != date_std_devs.end() && !it->second.empty()) {
                const std::vector<double>& stddevs = it->second;
                double avg_stddev = TMath::Mean(stddevs.size(), stddevs.data());
                date_values_stddev.push_back(date_cat);
                date_avg_stddevs.push_back(avg_stddev);
                std::cout << "Date " << date_labels[date_cat] << " (Category " << date_cat << "): Average Std Dev = " << avg_stddev << " (N=" << stddevs.size() << ")" << std::endl;
            }
        }

        if (date_values_stddev.empty() || date_avg_stddevs.empty()) {
            std::cout << "No data available to plot Date vs Average Std Dev." << std::endl;
        } else {
             TGraph* graph_date_stddev = new TGraph(date_values_stddev.size(), date_values_stddev.data(), date_avg_stddevs.data());
             graph_date_stddev->SetTitle("Average Std Dev vs Date; Date; Average Std Dev of Amplitude (Column B)");
             graph_date_stddev->SetMarkerStyle(22);
             graph_date_stddev->SetMarkerSize(1.2);
             graph_date_stddev->SetMarkerColor(kCyan+2);
             graph_date_stddev->SetLineColor(kCyan+2);
             graph_date_stddev->SetLineWidth(2);

             // Create Canvas
             TCanvas* canvas_date_stddev = new TCanvas("c_date_vs_avg_stddev", "Date vs Avg Std Dev", 800, 600);
             canvas_date_stddev->SetGrid();
             graph_date_stddev->Draw("AP");

             // Improve axes
             graph_date_stddev->GetXaxis()->SetLimits(*date_values_stddev.begin() - 1, *date_values_stddev.rbegin() + 1);
             for (size_t i = 0; i < date_values_stddev.size(); ++i) {
                 graph_date_stddev->GetXaxis()->SetBinLabel(i+1, date_labels[date_values_stddev[i]].c_str());
             }
             double max_y_date_stddev = *std::max_element(date_avg_stddevs.begin(), date_avg_stddevs.end());
             double min_y_date_stddev = *std::min_element(date_avg_stddevs.begin(), date_avg_stddevs.end());
             double range_y_date_stddev = max_y_date_stddev - min_y_date_stddev;
             if (range_y_date_stddev > 0) {
                 graph_date_stddev->GetYaxis()->SetRangeUser((min_y_date_stddev - range_y_date_stddev * 0.05), (max_y_date_stddev + range_y_date_stddev * 0.05));
             } else {
                 graph_date_stddev->GetYaxis()->SetRangeUser(min_y_date_stddev * 0.95, max_y_date_stddev * 1.05);
             }
             graph_date_stddev->GetXaxis()->SetTitle("Date");
             graph_date_stddev->GetXaxis()->CenterTitle();

             // Save to PDF
             TString plot_filename_date_stddev = "date_vs_avg_stddev_per_row.pdf";
             canvas_date_stddev->SaveAs(plot_filename_date_stddev);
             std::cout << "Plot saved to '" << plot_filename_date_stddev << "'." << std::endl;

             // Clean up
             delete graph_date_stddev;
             delete canvas_date_stddev;
        }


        // --- End of NEW DATE PLOTS ---
    }

    std::cout << "=============================================================" << std::endl;
    std::cout << "Analysis complete." << std::endl;
    std::cout << "=============================================================" << std::endl;
}