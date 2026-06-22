//
// File: time_vs_columns_density_by_kv.C
//
// Description:
// This ROOT C++ macro scans the current directory for CSV files (.csv).
// It reads time (Column A, exponential notation) and amplitudes from Columns B (Trigger),
// C (Ch1), D (Ch2), and E (Ch3).
// It categorizes files based on the 3rd digit of their numeric name:
//   - 3rd digit '0' -> 8kV, '1' -> 9kV, ..., '6' -> 14kV.
//   - Files with < 3 digits are treated as having a 3rd digit of '0'.
// It then plots the following combinations on SEPARATE graphs for EACH kV level (8-14kV):
//   - Graph 1: Time (A) vs Trigger (Binned Density) and Time (A) vs Ch1 (Binned Density)
//   - Graph 2: Time (A) vs Trigger (Binned Density) and Time (A) vs Ch2 (Binned Density)
//   - Graph 3: Time (A) vs Trigger (Binned Density) and Time (A) vs Ch3 (Binned Density)
// Data from all .csv files belonging to a specific kV category is displayed on that kV's shared graphs.
// Instead of plotting individual points, 2D histograms (TH2F) are used.
// Color intensity represents the number of data points falling into each bin (density).
// All plots are saved to a SINGLE PAGE PDF file in a grid layout.
// The overlay shows density for Trigger and ChX in the same plot.
// Axis labels are minimized for clarity.
//
// Dependencies:
// This script uses standard C++ file I/O for CSV parsing.
// It uses ROOT for plotting (TCanvas, TPad, TH2F, TAxis, TColor, TStyle).
//
// How to Run in ROOT (Ubuntu):
// 1. Launch the ROOT interactive terminal:
//    root
//
// 2. Compile and execute this macro from the ROOT prompt:
//    .L time_vs_columns_density_by_kv.C+
//    time_vs_columns_density_by_kv()
//

// --- Include necessary ROOT headers ---
#include <TSystem.h>
#include <TString.h>
#include <TCanvas.h>
#include <TPad.h>
#include <TH2F.h> // For 2D histograms
#include <TStyle.h> // For gStyle
#include <TAxis.h>
#include <TMath.h>
#include <TColor.h> // For TColor::GetColor
#include <iostream>
#include <vector>
#include <cmath>
#include <fstream>      // For CSV file reading
#include <string>       // For string manipulation
#include <sstream>      // For parsing lines
#include <limits>       // For numeric limits
#include <map>          // For storing density map and file data
#include <utility>      // For std::pair
#include <set>          // For storing unique kV categories found
#include <algorithm>    // For std::sort

// --- Configuration for Histogram Binning ---
// Adjust these numbers to change the "resolution" (number of bins)
const int NUM_BINS_X = 200; // Number of bins along the Time (X) axis
const int NUM_BINS_Y = 200; // Number of bins along the Amplitude (Y) axis

// --- Function Declarations ---
void time_vs_columns_density_by_kv();

// Helper function to read data from a single CSV file (reads from row 1 onwards)
bool read_csv_data_all_cols(const char* filename,
                            std::vector<double>& time,
                            std::vector<double>& col_Trigger, // Renamed B
                            std::vector<double>& col_Ch1,     // Renamed C
                            std::vector<double>& col_Ch2,     // Renamed D
                            std::vector<double>& col_Ch3);    // Renamed E

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
void time_vs_columns_density_by_kv() {
    std::cout << "Starting analysis: Time vs Specific Columns with Binned Density Coloring (Single Page PDF), grouped by kV..." << std::endl;
    std::cout << "kV categories determined by the 3rd digit of the numeric filename (0->8kV, 1->9kV, ..., 6->14kV)." << std::endl;
    std::cout << "Reading data from all .csv files in the current directory." << std::endl;
    std::cout << "Columns: A=Time, B=Trigger, C=Ch1, D=Ch2, E=Ch3" << std::endl;
    std::cout << "Plotting combinations using binned density (TH2F) for each kV (8-14):" << std::endl;
    std::cout << "  For each kV: 1. Time vs Trigger & Ch1, 2. Time vs Trigger & Ch2, 3. Time vs Trigger & Ch3" << std::endl;
    std::cout << "  Using " << NUM_BINS_X << " x " << NUM_BINS_Y << " bins for density representation." << std::endl;
    std::cout << "Plots will be saved to 'time_vs_columns_density_by_kv_output.pdf' (ALL on ONE page)." << std::endl;
    std::cout << "Overlaid density plots. No global palette or large external labels." << std::endl;

    // Map to store filename to data mapping for files with numeric names
    std::map<int, std::string> numeric_files; // index -> filename
    // Map to store data for each numeric file
    std::map<int, std::vector<double>> file_times; // index -> time data
    std::map<int, std::vector<double>> file_Trigger; // index -> Trigger data (B)
    std::map<int, std::vector<double>> file_Ch1;     // index -> Ch1 data (C)
    std::map<int, std::vector<double>> file_Ch2;     // index -> Ch2 data (D)
    std::map<int, std::vector<double>> file_Ch3;     // index -> Ch3 data (E)

    // Map to store kV category for each numeric file
    std::map<int, int> file_kV; // index -> kV category

    // Get the current directory
    TString current_dir = gSystem->pwd();
    std::cout << "Current directory: " << current_dir << std::endl;
    void* dir_handle = gSystem->OpenDirectory(current_dir);
    if (!dir_handle) {
        std::cerr << "Error: Could not open current directory." << std::endl;
        return;
    }

    const char* entry;
    int total_csv_files = 0;
    int files_processed_successfully = 0;

    // --- PASS 1: Read all CSV files with numeric names, collect data, and determine kV ---
    std::cout << "\n=== PASS 1: Reading CSV files with numeric names ===" << std::endl;
    while ((entry = gSystem->GetDirEntry(dir_handle))) {
        TString filename(entry);
        if (filename.EndsWith(".csv")) {
            total_csv_files++;
            int file_index = extract_numeric_index(filename.Data());
            if (file_index >= 0) { // Only process files with numeric names
                std::cout << "Reading CSV file: " << filename << " (index: " << file_index << ")" << std::endl;

                std::vector<double> file_time, col_Trigger, col_Ch1, col_Ch2, col_Ch3;
                if (read_csv_data_all_cols(filename.Data(), file_time, col_Trigger, col_Ch1, col_Ch2, col_Ch3)) {
                    files_processed_successfully++;

                    // Store data for this file
                    numeric_files[file_index] = filename.Data();
                    file_times[file_index] = file_time;
                    file_Trigger[file_index] = col_Trigger;
                    file_Ch1[file_index] = col_Ch1;
                    file_Ch2[file_index] = col_Ch2;
                    file_Ch3[file_index] = col_Ch3;

                    // Determine and store kV category
                    int kV_cat = extract_kV_category(file_index);
                    file_kV[file_index] = kV_cat;

                    std::cout << "  -> Successfully read " << file_time.size() << " data points." << std::endl;
                    if (kV_cat >= 8 && kV_cat <= 14) {
                        std::cout << "  -> Assigned to kV category: " << kV_cat << "kV" << std::endl;
                    } else {
                        std::cout << "  -> Assigned to invalid kV category: " << kV_cat << " (will be skipped in plotting)" << std::endl;
                    }
                } else {
                    std::cerr << "  -> Failed to read valid data from " << filename << std::endl;
                }
            } else {
                 std::cout << "Skipping non-numeric file: " << filename << std::endl;
            }
        }
    }
    gSystem->FreeDirectory(dir_handle);

    if (numeric_files.empty()) {
        std::cerr << "Error: No CSV files with numeric names were found or successfully read." << std::endl;
        return;
    }

    std::cout << "\nSuccessfully processed " << files_processed_successfully << " files with numeric names." << std::endl;

    // --- PASS 2: Group files by kV category and process ---
    std::cout << "\n=== PASS 2: Grouping by kV and generating binned plots ===" << std::endl;

    // Determine the set of kV categories present among valid files
    std::set<int> kV_categories_present;
    for (const auto& pair : file_kV) {
        int kV_cat = pair.second;
        if (kV_cat >= 8 && kV_cat <= 14) { // Only consider valid kV categories
             kV_categories_present.insert(kV_cat);
        }
    }

    if (kV_categories_present.empty()) {
        std::cout << "No files found belonging to valid kV categories (8-14kV)." << std::endl;
        return;
    }

    // Sort kV categories for consistent order
    std::vector<int> sorted_kVs(kV_categories_present.begin(), kV_categories_present.end());
    std::sort(sorted_kVs.begin(), sorted_kVs.end());

    std::cout << "Found kV categories: ";
    for (int kV : sorted_kVs) {
         std::cout << kV << " ";
    }
    std::cout << std::endl;

    // --- PASS 3: Create Histograms, Fill, Plot, and Save to Single Page PDF ---
    std::cout << "\n=== PASS 3: Creating binned plots and saving to SINGLE PAGE PDF ===" << std::endl;

    // --- Configuration for Single Page Layout ---
    int n_kVs = sorted_kVs.size();
    int n_cols = 3; // Ch1, Ch2, Ch3 comparisons (Trigger overlaid)
    int n_rows = n_kVs;

    // Create a large canvas for the combined PDF page - Increased size significantly
    int canvas_width = 2400; // 3 cols * 800px
    int canvas_height = 600 * n_rows; // n_rows * 600px
    TCanvas *pdf_canvas = new TCanvas("pdf_canvas", "Combined Binned Density Plots", canvas_width, canvas_height);
    // Remove global right margin for palette - no global palette
    pdf_canvas->SetRightMargin(0.05);

    // --- Process each kV category and create plots ---
    for (int row = 0; row < n_rows; ++row) {
        int kV = sorted_kVs[row];
        std::cout << "\n--- Processing kV Category: " << kV << "kV (Row " << (row+1) << ") ---" << std::endl;

        // Collect data for this kV category
        std::vector<double> kV_times, kV_Trigger, kV_Ch1, kV_Ch2, kV_Ch3;

        // Iterate through files and collect data belonging to this kV
        for (const auto& file_pair : numeric_files) {
            int file_index = file_pair.first;
            auto kV_it = file_kV.find(file_index);
            if (kV_it != file_kV.end() && kV_it->second == kV) {
                const std::vector<double>& times = file_times[file_index];
                const std::vector<double>& Trigger = file_Trigger[file_index];
                const std::vector<double>& Ch1 = file_Ch1[file_index];
                const std::vector<double>& Ch2 = file_Ch2[file_index];
                const std::vector<double>& Ch3 = file_Ch3[file_index];

                kV_times.insert(kV_times.end(), times.begin(), times.end());
                kV_Trigger.insert(kV_Trigger.end(), Trigger.begin(), Trigger.end());
                kV_Ch1.insert(kV_Ch1.end(), Ch1.begin(), Ch1.end());
                kV_Ch2.insert(kV_Ch2.end(), Ch2.begin(), Ch2.end());
                kV_Ch3.insert(kV_Ch3.end(), Ch3.begin(), Ch3.end());
            }
        }

        if (kV_times.empty()) {
            std::cout << "  -> No valid data found for kV " << kV << "kV. Skipping plots." << std::endl;
            continue;
        }

        std::cout << "  -> Collected " << kV_times.size() << " data points for kV " << kV << "kV." << std::endl;

        // --- Determine common plot ranges for this kV ---
        double x_min = *std::min_element(kV_times.begin(), kV_times.end());
        double x_max = *std::max_element(kV_times.begin(), kV_times.end());
        double y_min_Trigger = *std::min_element(kV_Trigger.begin(), kV_Trigger.end());
        double y_max_Trigger = *std::max_element(kV_Trigger.begin(), kV_Trigger.end());
        double y_min_Ch1 = *std::min_element(kV_Ch1.begin(), kV_Ch1.end());
        double y_max_Ch1 = *std::max_element(kV_Ch1.begin(), kV_Ch1.end());
        double y_min_Ch2 = *std::min_element(kV_Ch2.begin(), kV_Ch2.end());
        double y_max_Ch2 = *std::max_element(kV_Ch2.begin(), kV_Ch2.end());
        double y_min_Ch3 = *std::min_element(kV_Ch3.begin(), kV_Ch3.end());
        double y_max_Ch3 = *std::max_element(kV_Ch3.begin(), kV_Ch3.end());

        double y_min_overall = std::min({y_min_Trigger, y_min_Ch1, y_min_Ch2, y_min_Ch3});
        double y_max_overall = std::max({y_max_Trigger, y_max_Ch1, y_max_Ch2, y_max_Ch3});

        double x_range = x_max - x_min;
        double y_range = y_max_overall - y_min_overall;
        const double margin = 0.05;
        x_min -= margin * x_range; x_max += margin * x_range;
        y_min_overall -= margin * y_range; y_max_overall += margin * y_range;
        if (x_range == 0) { x_min -= 1e-10; x_max += 1e-10; }
        if (y_range == 0) { y_min_overall -= 1e-10; y_max_overall += 1e-10; }


        // --- Loop through the 3 comparisons for this kV ---
        for (int col = 0; col < n_cols; ++col) {
            // Calculate pad coordinates (normalized to 0-1) - Increased size
            // Leave minimal space for margins
            double pad_height_fraction = 0.95 / n_rows; // 95% for plots, 5% for top/bottom margins
            double pad_width_fraction = 0.95 / n_cols; // 95% for plots, 5% for left/right margins
            double x1 = 0.025 + col * pad_width_fraction; // 2.5% left margin
            double x2 = 0.025 + (col + 1) * pad_width_fraction;
            // Invert Y axis for correct row order (top row = kV 8)
            double y1 = 0.025 + (n_rows - 1 - row) * pad_height_fraction; // 2.5% bottom margin
            double y2 = 0.025 + (n_rows - row) * pad_height_fraction;

            TPad *pad = new TPad(Form("pad_kv%d_col%d", kV, col), "", x1, y1, x2, y2);
            pad->Draw();
            pad->cd();
            pad->SetGrid();
            // Minimal margins for individual pads to maximize plot area
            pad->SetLeftMargin(0.12);
            pad->SetRightMargin(0.12);
            pad->SetBottomMargin(0.12);
            pad->SetTopMargin(0.05); // Small top margin

            // Pointers for histograms in this pad
            TH2F *hTrigger = nullptr;
            TH2F *hOther = nullptr;
            TString other_label;
            std::vector<double> *other_data_ptr = nullptr;

            // Create histograms based on column comparison
            TString hist_name_Trigger = Form("hTrigger_kv%d_col%d", kV, col);
            TString hist_name_Other = Form("hOther_kv%d_col%d", kV, col);
            // Title only shows kV for minimalism
            TString hist_title = Form("kV %d", kV);

            hTrigger = new TH2F(hist_name_Trigger, hist_title, NUM_BINS_X, x_min, x_max, NUM_BINS_Y, y_min_overall, y_max_overall);

            if (col == 0) { // Trigger vs Ch1
                hOther = new TH2F(hist_name_Other, hist_title, NUM_BINS_X, x_min, x_max, NUM_BINS_Y, y_min_overall, y_max_overall);
                other_label = "Ch1";
                other_data_ptr = &kV_Ch1;
            } else if (col == 1) { // Trigger vs Ch2
                hOther = new TH2F(hist_name_Other, hist_title, NUM_BINS_X, x_min, x_max, NUM_BINS_Y, y_min_overall, y_max_overall);
                other_label = "Ch2";
                other_data_ptr = &kV_Ch2;
            } else if (col == 2) { // Trigger vs Ch3
                hOther = new TH2F(hist_name_Other, hist_title, NUM_BINS_X, x_min, x_max, NUM_BINS_Y, y_min_overall, y_max_overall);
                other_label = "Ch3";
                other_data_ptr = &kV_Ch3;
            }

            // Fill histograms
            if (hTrigger && hOther && other_data_ptr) {
                for (size_t i = 0; i < kV_times.size(); ++i) {
                    if (is_valid_number(kV_times[i]) && is_valid_number(kV_Trigger[i])) {
                        hTrigger->Fill(kV_times[i], kV_Trigger[i]);
                    }
                    if (is_valid_number(kV_times[i]) && is_valid_number((*other_data_ptr)[i])) {
                        hOther->Fill(kV_times[i], (*other_data_ptr)[i]);
                    }
                }

                // Style histograms
                hTrigger->SetStats(0); // Disable stats box
                // Minimal axis labels
                hTrigger->GetXaxis()->SetTitle("Time");
                hTrigger->GetYaxis()->SetTitle(Form("Amplitude (%s/Trig)", other_label.Data()));
                // Reduced label and title sizes for clarity within larger plots
                hTrigger->GetXaxis()->SetLabelSize(0.05);
                hTrigger->GetYaxis()->SetLabelSize(0.05);
                hTrigger->GetXaxis()->SetTitleSize(0.06);
                hTrigger->GetYaxis()->SetTitleSize(0.06);
                hTrigger->GetXaxis()->SetTitleOffset(0.9);
                hTrigger->GetYaxis()->SetTitleOffset(1.0);
                // Draw Trigger first (background)
                hTrigger->Draw("COL"); // COL draws bins with color based on content

                // Draw Other on top (foreground)
                hOther->SetStats(0); // Disable stats box
                // Axis labels handled by hTrigger
                hOther->Draw("COL SAME"); // SAME draws on the same pad

                std::cout << "  -> Created overlay plot for kV " << kV << "kV, Trigger vs " << other_label << " (Pad row=" << (row+1) << ", col=" << (col+1) << ")." << std::endl;
            } else {
                std::cerr << "  -> Error: Failed to create histograms for kV " << kV << "kV, column " << col << "." << std::endl;
            }

            // Update pad
            pad->Update();
            pdf_canvas->cd(); // Go back to main canvas
        } // End loop over columns (comparisons) for this kV
    } // End loop over kV categories (rows)

    // --- Save the Single Page PDF ---
    pdf_canvas->Update();
    TString pdf_filename = "time_vs_columns_density_by_kv_output.pdf";
    pdf_canvas->Print(pdf_filename + "["); // Open PDF
    pdf_canvas->Print(pdf_filename);       // Add this page
    pdf_canvas->Print(pdf_filename + "]"); // Close PDF
    std::cout << "\nCombined PDF '" << pdf_filename << "' created successfully with ALL plots on ONE page." << std::endl;
    std::cout << "Layout: " << n_rows << " rows (kV), " << n_cols << " columns (Trigger vs Ch1/Ch2/Ch3)." << std::endl;
    std::cout << "Overlaid density plots. No global palette or large external labels." << std::endl;
    std::cout << "Canvas size increased for better zoom capability." << std::endl;

    // Clean up the main canvas
    delete pdf_canvas;

    std::cout << "\nBinned analysis and single-page plotting complete for all kV categories." << std::endl;
    std::cout << "Note: Color represents the number of data points falling into each bin (density)." << std::endl;
    std::cout << "      Resolution is controlled by NUM_BINS_X and NUM_BINS_Y." << std::endl;
    std::cout << "The PDF is '" << pdf_filename << "'." << std::endl;
}


// --- Helper Function Implementations ---

/**
 * @brief Reads data from a specified .csv file, extracting Columns A, B (Trigger), C (Ch1), D (Ch2), E (Ch3).
 * @param filename The name of the CSV file.
 * @param time Vector to store time data (Column A).
 * @param col_Trigger Vector to store data from Column B (Trigger).
 * @param col_Ch1 Vector to store data from Column C (Ch1).
 * @param col_Ch2 Vector to store data from Column D (Ch2).
 * @param col_Ch3 Vector to store data from Column E (Ch3).
 * @return true if successful (at least one valid row processed), false otherwise.
 */
bool read_csv_data_all_cols(const char* filename,
                            std::vector<double>& time,
                            std::vector<double>& col_Trigger,
                            std::vector<double>& col_Ch1,
                            std::vector<double>& col_Ch2,
                            std::vector<double>& col_Ch3) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error opening CSV file: " << filename << std::endl;
        return false;
    }

    time.clear();
    col_Trigger.clear();
    col_Ch1.clear();
    col_Ch2.clear();
    col_Ch3.clear();

    std::string line;
    int current_line_num = 0;
    const int DATA_START_ROW = 1; // Read from the very first data row (0-indexed)

    int valid_rows = 0;
    int invalid_rows = 0;

    while (std::getline(file, line)) {
        current_line_num++;
        if (current_line_num <= DATA_START_ROW) {
            continue; // Skip potential header rows
        }
        if (trim(line).empty()) {
            continue; // Skip empty lines
        }

        std::stringstream ss(line);
        std::string segment;
        std::vector<double> row_values;

        // Parse up to 5 columns (A, B, C, D, E)
        int col_count = 0;
        while(col_count < 5 && std::getline(ss, segment, ',')) {
            double value;
            if (safe_string_to_double(trim(segment), value)) {
                row_values.push_back(value);
            } else {
                row_values.push_back(std::numeric_limits<double>::quiet_NaN()); // Mark invalid entries
            }
            col_count++;
        }

        // Need at least A and B (indices 0, 1) to be considered. C,D,E are indices 2,3,4.
        if (row_values.size() >= 2) {
            double current_time = row_values[0]; // Column A (0-indexed)
            double current_Trigger = row_values[1];    // Column B (0-indexed) - Renamed
            double current_Ch1 = (row_values.size() > 2) ? row_values[2] : std::numeric_limits<double>::quiet_NaN(); // Renamed
            double current_Ch2 = (row_values.size() > 3) ? row_values[3] : std::numeric_limits<double>::quiet_NaN(); // Renamed
            double current_Ch3 = (row_values.size() > 4) ? row_values[4] : std::numeric_limits<double>::quiet_NaN(); // Renamed

            // Check if time and Trigger are valid (minimum requirement)
            if (is_valid_number(current_time) && is_valid_number(current_Trigger)) {
                // Store data. Store NaN for Ch1/Ch2/Ch3 if they were missing or invalid.
                time.push_back(current_time);
                col_Trigger.push_back(current_Trigger);
                col_Ch1.push_back(current_Ch1);
                col_Ch2.push_back(current_Ch2);
                col_Ch3.push_back(current_Ch3);
                valid_rows++;
            } else {
                 invalid_rows++; // Time or Trigger invalid
            }
        } else {
            invalid_rows++; // Not enough columns (need at least A and B)
        }
    }
    file.close();

    if (valid_rows == 0) {
        std::cout << "  -> Warning: No valid data rows found in file " << filename << std::endl;
        return false; // Return false if no valid data was found
    } else {
        std::cout << "  -> Read " << valid_rows << " valid rows." << std::endl;
        return true;
    }
}

/**
 * @brief Validates if a number is finite and not NaN.
 * @param value The value to check.
 * @return true if the value is valid, false otherwise.
 */
bool is_valid_number(double value) {
    // Use standard C++ and ROOT's TMath for robustness
    return std::isfinite(value) && !std::isnan(value) && !TMath::IsNaN(value);
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
        // stod can handle exponential notation like -1.70E-7
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
        return -1;
    }
}