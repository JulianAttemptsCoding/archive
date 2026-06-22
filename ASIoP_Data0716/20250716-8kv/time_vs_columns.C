//
// File: time_vs_columns_density.C
//
// Description:
// This ROOT C++ macro scans the current directory for CSV files (.csv).
// It reads time (Column A, exponential notation) and amplitudes from Columns B, C, D, and E.
// It plots the following combinations on separate graphs:
//   - Graph 1: Time (A) vs Column B (Red Gradient) and Time (A) vs Column C (Blue Gradient)
//   - Graph 2: Time (A) vs Column B (Red Gradient) and Time (A) vs Column D (Blue Gradient)
//   - Graph 3: Time (A) vs Column B (Red Gradient) and Time (A) vs Column E (Blue Gradient)
// Data from all .csv files in the folder is displayed on the shared graphs.
// Points use default markers. The color intensity represents the log of the "density"
// (number of times that specific (Time, Amplitude) pair appears at that location).
// Color gradient: Blue/Yellow (least/most dense).
// One dataset (B) uses a Red-based gradient, the other (C/D/E) uses a Blue-based gradient.
//
// Dependencies:
// This script uses standard C++ file I/O for CSV parsing.
// It uses ROOT for plotting (TCanvas, TGraph, TAxis, TLegend, TColor).
//
// How to Run in ROOT (Ubuntu):
// 1. Launch the ROOT interactive terminal:
//    root
//
// 2. Compile and execute this macro from the ROOT prompt:
//    .L time_vs_columns_density.C+
//    time_vs_columns_density()
//

#include <TSystem.h>
#include <TString.h>
#include <TCanvas.h>
#include <TGraph.h>
#include <TAxis.h>
#include <TLegend.h>
#include <TMath.h>
#include <TH1F.h> // Correct header for TH1F (returned by DrawFrame)
#include <TColor.h> // For TColor::GetColor
#include <iostream>
#include <vector>
#include <cmath>
#include <fstream>      // For CSV file reading
#include <string>       // For string manipulation
#include <sstream>      // For parsing lines
#include <limits>       // For numeric limits
#include <map>          // For storing density map
#include <utility>      // For std::pair

// --- Function Declarations ---
void time_vs_columns_density();

// Helper function to read data from a single CSV file (reads from row 1 onwards)
bool read_csv_data_all_cols(const char* filename,
                            std::vector<double>& time,
                            std::vector<double>& col_B,
                            std::vector<double>& col_C,
                            std::vector<double>& col_D,
                            std::vector<double>& col_E);

// Helper function to validate numeric values
bool is_valid_number(double value);

// Helper function to trim whitespace from strings
std::string trim(const std::string& str);

// Helper function to safely parse string to double
bool safe_string_to_double(const std::string& str, double& result);

// Helper function to create a color based on log density for B (Red gradient)
Color_t get_color_for_B_density(double log_density, double min_log_density, double max_log_density);

// Helper function to create a color based on log density for C/D/E (Blue gradient)
Color_t get_color_for_CDE_density(double log_density, double min_log_density, double max_log_density);

// --- Main Processing Function ---
void time_vs_columns_density() {
    std::cout << "Starting analysis: Time vs Specific Columns with Density Coloring..." << std::endl;
    std::cout << "Reading data from all .csv files in the current directory." << std::endl;
    std::cout << "Plotting combinations with density coloring (log(count) -> Blue/Yellow):" << std::endl;
    std::cout << "  1. Time (A) vs Column B (Red Gradient) and Time (A) vs Column C (Blue Gradient)" << std::endl;
    std::cout << "  2. Time (A) vs Column B (Red Gradient) and Time (A) vs Column D (Blue Gradient)" << std::endl;
    std::cout << "  3. Time (A) vs Column B (Red Gradient) and Time (A) vs Column E (Blue Gradient)" << std::endl;

    // Vectors to store ALL data from ALL files for each column
    std::vector<double> all_times;     // Combined times from all files
    std::vector<double> all_B;         // Combined Column B data
    std::vector<double> all_C;         // Combined Column C data
    std::vector<double> all_D;         // Combined Column D data
    std::vector<double> all_E;         // Combined Column E data

    // Maps to count occurrences of each (time, amplitude) pair for density calculation
    // Key: pair<time, amplitude>, Value: count
    std::map<std::pair<double, double>, int> density_map_B;
    std::map<std::pair<double, double>, int> density_map_C;
    std::map<std::pair<double, double>, int> density_map_D;
    std::map<std::pair<double, double>, int> density_map_E;

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

    // --- PASS 1: Read all CSV files and collect data ---
    std::cout << "\n=== PASS 1: Reading all CSV files ===" << std::endl;
    while ((entry = gSystem->GetDirEntry(dir_handle))) {
        TString filename(entry);
        if (filename.EndsWith(".csv")) {
            total_csv_files++;
            std::cout << "Reading CSV file: " << filename << std::endl;

            std::vector<double> file_time, file_B, file_C, file_D, file_E;
            if (read_csv_data_all_cols(filename.Data(), file_time, file_B, file_C, file_D, file_E)) {
                files_processed_successfully++;

                // Populate density maps
                for (size_t i = 0; i < file_time.size(); ++i) {
                    density_map_B[std::make_pair(file_time[i], file_B[i])]++;
                    density_map_C[std::make_pair(file_time[i], file_C[i])]++;
                    density_map_D[std::make_pair(file_time[i], file_D[i])]++;
                    density_map_E[std::make_pair(file_time[i], file_E[i])]++;
                }

                // Append data from this file to the global collections
                all_times.insert(all_times.end(), file_time.begin(), file_time.end());
                all_B.insert(all_B.end(), file_B.begin(), file_B.end());
                all_C.insert(all_C.end(), file_C.begin(), file_C.end());
                all_D.insert(all_D.end(), file_D.begin(), file_D.end());
                all_E.insert(all_E.end(), file_E.begin(), file_E.end());

                std::cout << "  -> Read data points: B(" << file_B.size() << "), C(" << file_C.size()
                          << "), D(" << file_D.size() << "), E(" << file_E.size() << ")" << std::endl;
            } else {
                std::cerr << "  -> Failed to read valid data from " << filename << std::endl;
            }
        }
    }
    gSystem->FreeDirectory(dir_handle);

    if (files_processed_successfully == 0 || all_times.empty()) {
        std::cerr << "Error: No valid data was read from any CSV files." << std::endl;
        return;
    }

    std::cout << "\nSuccessfully processed " << files_processed_successfully << " CSV files." << std::endl;
    std::cout << "Total data points collected: Time(" << all_times.size() << "), B(" << all_B.size()
              << "), C(" << all_C.size() << "), D(" << all_D.size() << "), E(" << all_E.size() << ")" << std::endl;

    // Basic size check (they should all be the same size as time)
    if (all_times.size() != all_B.size() || all_times.size() != all_C.size() ||
        all_times.size() != all_D.size() || all_times.size() != all_E.size()) {
        std::cerr << "Error: Mismatch in data vector sizes after reading files." << std::endl;
        return;
    }

    // --- PASS 2: Determine Density Ranges for Coloring ---
    std::cout << "\n=== PASS 2: Calculating Density Ranges ===" << std::endl;

    // Find min and max log density for B
    double min_log_density_B = std::numeric_limits<double>::max();
    double max_log_density_B = std::numeric_limits<double>::lowest();
    for (const auto& pair : density_map_B) {
        int count = pair.second;
        if (count > 0) {
            double log_density = std::log10(static_cast<double>(count));
            if (log_density < min_log_density_B) min_log_density_B = log_density;
            if (log_density > max_log_density_B) max_log_density_B = log_density;
        }
    }
    if (min_log_density_B > max_log_density_B) {
        min_log_density_B = max_log_density_B = 0; // Handle case where no points or all counts are 0/1
        std::cout << "Warning: No valid density data found for Column B or all counts are 1." << std::endl;
    } else {
        std::cout << "Column B Log Density Range: [" << min_log_density_B << ", " << max_log_density_B << "]" << std::endl;
    }

    // Find min and max log density for C
    double min_log_density_C = std::numeric_limits<double>::max();
    double max_log_density_C = std::numeric_limits<double>::lowest();
    for (const auto& pair : density_map_C) {
        int count = pair.second;
        if (count > 0) {
            double log_density = std::log10(static_cast<double>(count));
            if (log_density < min_log_density_C) min_log_density_C = log_density;
            if (log_density > max_log_density_C) max_log_density_C = log_density;
        }
    }
    if (min_log_density_C > max_log_density_C) {
        min_log_density_C = max_log_density_C = 0;
        std::cout << "Warning: No valid density data found for Column C or all counts are 1." << std::endl;
    } else {
         std::cout << "Column C Log Density Range: [" << min_log_density_C << ", " << max_log_density_C << "]" << std::endl;
    }

    // Find min and max log density for D
    double min_log_density_D = std::numeric_limits<double>::max();
    double max_log_density_D = std::numeric_limits<double>::lowest();
    for (const auto& pair : density_map_D) {
        int count = pair.second;
        if (count > 0) {
            double log_density = std::log10(static_cast<double>(count));
            if (log_density < min_log_density_D) min_log_density_D = log_density;
            if (log_density > max_log_density_D) max_log_density_D = log_density;
        }
    }
    if (min_log_density_D > max_log_density_D) {
        min_log_density_D = max_log_density_D = 0;
        std::cout << "Warning: No valid density data found for Column D or all counts are 1." << std::endl;
    } else {
         std::cout << "Column D Log Density Range: [" << min_log_density_D << ", " << max_log_density_D << "]" << std::endl;
    }

    // Find min and max log density for E
    double min_log_density_E = std::numeric_limits<double>::max();
    double max_log_density_E = std::numeric_limits<double>::lowest();
    for (const auto& pair : density_map_E) {
        int count = pair.second;
        if (count > 0) {
            double log_density = std::log10(static_cast<double>(count));
            if (log_density < min_log_density_E) min_log_density_E = log_density;
            if (log_density > max_log_density_E) max_log_density_E = log_density;
        }
    }
    if (min_log_density_E > max_log_density_E) {
        min_log_density_E = max_log_density_E = 0;
        std::cout << "Warning: No valid density data found for Column E or all counts are 1." << std::endl;
    } else {
         std::cout << "Column E Log Density Range: [" << min_log_density_E << ", " << max_log_density_E << "]" << std::endl;
    }


    // --- PASS 3: Plotting ---
    std::cout << "\n=== PASS 3: Generating Plots with Density Coloring ===" << std::endl;

    const int NUM_POINTS = all_times.size();
    if (NUM_POINTS == 0) {
        std::cerr << "Error: No data points to plot." << std::endl;
        return;
    }
    const double* x_vals = all_times.data(); // Pointer to time data

    // --- Plot 1: Time vs B (Red Gradient) and Time vs C (Blue Gradient) ---
    TCanvas *c1 = new TCanvas("c1", "Time vs B and C (Density Colored)", 900, 700);
    c1->SetGrid();

    // Determine plot ranges
    double x_min = *std::min_element(all_times.begin(), all_times.end());
    double x_max = *std::max_element(all_times.begin(), all_times.end());
    double y_min_B = *std::min_element(all_B.begin(), all_B.end());
    double y_max_B = *std::max_element(all_B.begin(), all_B.end());
    double y_min_C = *std::min_element(all_C.begin(), all_C.end());
    double y_max_C = *std::max_element(all_C.begin(), all_C.end());
    double y_min = std::min(y_min_B, y_min_C);
    double y_max = std::max(y_max_B, y_max_C);
    double x_range = x_max - x_min;
    double y_range = y_max - y_min;
    x_min -= 0.05 * x_range; x_max += 0.05 * x_range;
    y_min -= 0.05 * y_range; y_max += 0.05 * y_range;
    if (x_range == 0) { x_min -= 1e-10; x_max += 1e-10; }
    if (y_range == 0) { y_min -= 1e-10; y_max += 1e-10; }

    // Create frame
    TH1F *frame1 = c1->DrawFrame(x_min, y_min, x_max, y_max, "Time vs B and C (Density Colored);Time (Column A);Amplitude");
    frame1->SetStats(0);

    // Plot B data (Red Gradient)
    for (int i = 0; i < NUM_POINTS; ++i) {
        auto key = std::make_pair(all_times[i], all_B[i]);
        int count = density_map_B.count(key) ? density_map_B[key] : 0;
        double log_density = (count > 0) ? std::log10(static_cast<double>(count)) : min_log_density_B;
        Color_t point_color = get_color_for_B_density(log_density, min_log_density_B, max_log_density_B);

        TGraph *g = new TGraph(1, &all_times[i], &all_B[i]);
        g->SetMarkerStyle(20); // Filled circle
        g->SetMarkerSize(0.7);
        g->SetMarkerColor(point_color);
        g->SetLineColor(point_color);
        g->SetLineStyle(0); // No line between points
        g->Draw("P SAME"); // P=Points, SAME=on top of frame
    }

    // Plot C data (Blue Gradient)
    for (int i = 0; i < NUM_POINTS; ++i) {
        auto key = std::make_pair(all_times[i], all_C[i]);
        int count = density_map_C.count(key) ? density_map_C[key] : 0;
        double log_density = (count > 0) ? std::log10(static_cast<double>(count)) : min_log_density_C;
        Color_t point_color = get_color_for_CDE_density(log_density, min_log_density_C, max_log_density_C);

        TGraph *g = new TGraph(1, &all_times[i], &all_C[i]);
        g->SetMarkerStyle(20); // Filled circle
        g->SetMarkerSize(0.7);
        g->SetMarkerColor(point_color);
        g->SetLineColor(point_color);
        g->SetLineStyle(0); // No line between points
        g->Draw("P SAME"); // P=Points, SAME=on top of frame
    }

    TLegend *legend1 = new TLegend(0.7, 0.8, 0.9, 0.9);
    legend1->AddEntry((TObject*)0, "Color: log(density)", ""); // Dummy entry for color explanation
    legend1->AddEntry((TObject*)0, "Blue -> Yellow", "");     // Dummy entry for color gradient
    legend1->AddEntry((TObject*)0, "", "");                   // Spacer
    TGraph *dummy_B = new TGraph(1); dummy_B->SetMarkerColor(kRed); dummy_B->SetMarkerStyle(20);
    TGraph *dummy_C = new TGraph(1); dummy_C->SetMarkerColor(kBlue); dummy_C->SetMarkerStyle(20);
    legend1->AddEntry(dummy_B, "Column B", "p");
    legend1->AddEntry(dummy_C, "Column C", "p");
    legend1->Draw();

    c1->Update();
    std::cout << "Plot 'c1' (Time vs B and C with Density) created." << std::endl;
    std::cout << "  B Color scale: Red Gradient (log(density)=" << min_log_density_B << " to " << max_log_density_B << ")" << std::endl;
    std::cout << "  C Color scale: Blue Gradient (log(density)=" << min_log_density_C << " to " << max_log_density_C << ")" << std::endl;


    // --- Plot 2: Time vs B (Red Gradient) and Time vs D (Blue Gradient) ---
    TCanvas *c2 = new TCanvas("c2", "Time vs B and D (Density Colored)", 900, 700);
    c2->SetGrid();

    // Determine plot ranges (re-use x_min, x_max, y_min_B, y_max_B)
    double y_min_D = *std::min_element(all_D.begin(), all_D.end());
    double y_max_D = *std::max_element(all_D.begin(), all_D.end());
    y_min = std::min({y_min_B, y_min_D});
    y_max = std::max({y_max_B, y_max_D});
    y_range = y_max - y_min;
    y_min -= 0.05 * y_range; y_max += 0.05 * y_range;
    if (y_range == 0) { y_min -= 1e-10; y_max += 1e-10; }

    // Create frame
    TH1F *frame2 = c2->DrawFrame(x_min, y_min, x_max, y_max, "Time vs B and D (Density Colored);Time (Column A);Amplitude");
    frame2->SetStats(0);

    // Plot B data (Red Gradient) - Re-plot
    for (int i = 0; i < NUM_POINTS; ++i) {
        auto key = std::make_pair(all_times[i], all_B[i]);
        int count = density_map_B.count(key) ? density_map_B[key] : 0;
        double log_density = (count > 0) ? std::log10(static_cast<double>(count)) : min_log_density_B;
        Color_t point_color = get_color_for_B_density(log_density, min_log_density_B, max_log_density_B);

        TGraph *g = new TGraph(1, &all_times[i], &all_B[i]);
        g->SetMarkerStyle(20);
        g->SetMarkerSize(0.7);
        g->SetMarkerColor(point_color);
        g->SetLineColor(point_color);
        g->SetLineStyle(0);
        g->Draw("P SAME");
    }

    // Plot D data (Blue Gradient)
    for (int i = 0; i < NUM_POINTS; ++i) {
        auto key = std::make_pair(all_times[i], all_D[i]);
        int count = density_map_D.count(key) ? density_map_D[key] : 0;
        double log_density = (count > 0) ? std::log10(static_cast<double>(count)) : min_log_density_D;
        Color_t point_color = get_color_for_CDE_density(log_density, min_log_density_D, max_log_density_D);

        TGraph *g = new TGraph(1, &all_times[i], &all_D[i]);
        g->SetMarkerStyle(20);
        g->SetMarkerSize(0.7);
        g->SetMarkerColor(point_color);
        g->SetLineColor(point_color);
        g->SetLineStyle(0);
        g->Draw("P SAME");
    }

    TLegend *legend2 = new TLegend(0.7, 0.8, 0.9, 0.9);
    legend2->AddEntry((TObject*)0, "Color: log(density)", "");
    legend2->AddEntry((TObject*)0, "Blue -> Yellow", "");
    legend2->AddEntry((TObject*)0, "", "");
    legend2->AddEntry(dummy_B, "Column B", "p");
    TGraph *dummy_D = new TGraph(1); dummy_D->SetMarkerColor(kBlue); dummy_D->SetMarkerStyle(20);
    legend2->AddEntry(dummy_D, "Column D", "p");
    legend2->Draw();

    c2->Update();
    std::cout << "Plot 'c2' (Time vs B and D with Density) created." << std::endl;
    std::cout << "  B Color scale: Red Gradient (log(density)=" << min_log_density_B << " to " << max_log_density_B << ")" << std::endl;
    std::cout << "  D Color scale: Blue Gradient (log(density)=" << min_log_density_D << " to " << max_log_density_D << ")" << std::endl;


    // --- Plot 3: Time vs B (Red Gradient) and Time vs E (Blue Gradient) ---
    TCanvas *c3 = new TCanvas("c3", "Time vs B and E (Density Colored)", 900, 700);
    c3->SetGrid();

    // Determine plot ranges (re-use x_min, x_max, y_min_B, y_max_B)
    double y_min_E = *std::min_element(all_E.begin(), all_E.end());
    double y_max_E = *std::max_element(all_E.begin(), all_E.end());
    y_min = std::min({y_min_B, y_min_E});
    y_max = std::max({y_max_B, y_max_E});
    y_range = y_max - y_min;
    y_min -= 0.05 * y_range; y_max += 0.05 * y_range;
    if (y_range == 0) { y_min -= 1e-10; y_max += 1e-10; }

    // Create frame
    TH1F *frame3 = c3->DrawFrame(x_min, y_min, x_max, y_max, "Time vs B and E (Density Colored);Time (Column A);Amplitude");
    frame3->SetStats(0);

     // Plot B data (Red Gradient) - Re-plot
    for (int i = 0; i < NUM_POINTS; ++i) {
        auto key = std::make_pair(all_times[i], all_B[i]);
        int count = density_map_B.count(key) ? density_map_B[key] : 0;
        double log_density = (count > 0) ? std::log10(static_cast<double>(count)) : min_log_density_B;
        Color_t point_color = get_color_for_B_density(log_density, min_log_density_B, max_log_density_B);

        TGraph *g = new TGraph(1, &all_times[i], &all_B[i]);
        g->SetMarkerStyle(20);
        g->SetMarkerSize(0.7);
        g->SetMarkerColor(point_color);
        g->SetLineColor(point_color);
        g->SetLineStyle(0);
        g->Draw("P SAME");
    }

    // Plot E data (Blue Gradient)
    for (int i = 0; i < NUM_POINTS; ++i) {
        auto key = std::make_pair(all_times[i], all_E[i]);
        int count = density_map_E.count(key) ? density_map_E[key] : 0;
        double log_density = (count > 0) ? std::log10(static_cast<double>(count)) : min_log_density_E;
        Color_t point_color = get_color_for_CDE_density(log_density, min_log_density_E, max_log_density_E);

        TGraph *g = new TGraph(1, &all_times[i], &all_E[i]);
        g->SetMarkerStyle(20);
        g->SetMarkerSize(0.7);
        g->SetMarkerColor(point_color);
        g->SetLineColor(point_color);
        g->SetLineStyle(0);
        g->Draw("P SAME");
    }

    TLegend *legend3 = new TLegend(0.7, 0.8, 0.9, 0.9);
    legend3->AddEntry((TObject*)0, "Color: log(density)", "");
    legend3->AddEntry((TObject*)0, "Blue -> Yellow", "");
    legend3->AddEntry((TObject*)0, "", "");
    legend3->AddEntry(dummy_B, "Column B", "p");
    TGraph *dummy_E = new TGraph(1); dummy_E->SetMarkerColor(kBlue); dummy_E->SetMarkerStyle(20);
    legend3->AddEntry(dummy_E, "Column E", "p");
    legend3->Draw();

    c3->Update();
    std::cout << "Plot 'c3' (Time vs B and E with Density) created." << std::endl;
    std::cout << "  B Color scale: Red Gradient (log(density)=" << min_log_density_B << " to " << max_log_density_B << ")" << std::endl;
    std::cout << "  E Color scale: Blue Gradient (log(density)=" << min_log_density_E << " to " << max_log_density_E << ")" << std::endl;

    std::cout << "\nAnalysis and plotting complete with density coloring." << std::endl;
    std::cout << "Note: Color represents the logarithm of the number of times a specific (Time, Amplitude) pair appears." << std::endl;
    std::cout << "      Blue indicates lower density, Yellow indicates higher density." << std::endl;
}


// --- Helper Function Implementations ---

/**
 * @brief Reads data from a specified .csv file, extracting Columns A, B, C, D, E.
 * @param filename The name of the CSV file.
 * @param time Vector to store time data (Column A).
 * @param col_B Vector to store data from Column B.
 * @param col_C Vector to store data from Column C.
 * @param col_D Vector to store data from Column D.
 * @param col_E Vector to store data from Column E.
 * @return true if successful (at least one valid row processed), false otherwise.
 */
bool read_csv_data_all_cols(const char* filename,
                            std::vector<double>& time,
                            std::vector<double>& col_B,
                            std::vector<double>& col_C,
                            std::vector<double>& col_D,
                            std::vector<double>& col_E) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error opening CSV file: " << filename << std::endl;
        return false;
    }

    // Clear output vectors
    time.clear();
    col_B.clear();
    col_C.clear();
    col_D.clear();
    col_E.clear();

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
            double current_B = row_values[1];    // Column B (0-indexed)
            double current_C = (row_values.size() > 2) ? row_values[2] : std::numeric_limits<double>::quiet_NaN();
            double current_D = (row_values.size() > 3) ? row_values[3] : std::numeric_limits<double>::quiet_NaN();
            double current_E = (row_values.size() > 4) ? row_values[4] : std::numeric_limits<double>::quiet_NaN();

            // Check if time and B are valid (minimum requirement)
            if (is_valid_number(current_time) && is_valid_number(current_B)) {
                // Store data. Store NaN for C/D/E if they were missing or invalid.
                time.push_back(current_time);
                col_B.push_back(current_B);
                col_C.push_back(current_C);
                col_D.push_back(current_D);
                col_E.push_back(current_E);
                valid_rows++;
            } else {
                 invalid_rows++; // Time or B invalid
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
 * @brief Gets a color for Column B based on the logarithm of density, scaled from dark red to yellow.
 * @param log_density The logarithm of the density count.
 * @param min_log_density The minimum log density found for B.
 * @param max_log_density The maximum log density found for B.
 * @return A ROOT Color_t value.
 */
Color_t get_color_for_B_density(double log_density, double min_log_density, double max_log_density) {
    // Handle case where all densities are the same
    if (min_log_density >= max_log_density) {
        // Return a default color, e.g., red
        return kRed;
    }

    // Normalize log_density to [0, 1]
    double norm_val = (log_density - min_log_density) / (max_log_density - min_log_density);
    norm_val = std::max(0.0, std::min(1.0, norm_val)); // Clamp to [0, 1]

    // Gradient from Dark Red to Yellow
    // Dark Red: (139, 0, 0) or approximately kRed+2 (darker red)
    // Yellow: (255, 255, 0) or kYellow
    // Interpolate RGB values
    int r = static_cast<int>(139 + (255 - 139) * norm_val); // 139 -> 255
    int g = static_cast<int>(0 + (255 - 0) * norm_val);     // 0 -> 255
    int b = static_cast<int>(0 + (0 - 0) * norm_val);       // 0 -> 0

    // Ensure values are within 0-255
    r = std::max(0, std::min(255, r));
    g = std::max(0, std::min(255, g));
    b = std::max(0, std::min(255, b));

    Int_t color_index = TColor::GetColor(r, g, b);
    return color_index;
}

/**
 * @brief Gets a color for Columns C/D/E based on the logarithm of density, scaled from blue to yellow.
 * @param log_density The logarithm of the density count.
 * @param min_log_density The minimum log density found for C/D/E.
 * @param max_log_density The maximum log density found for C/D/E.
 * @return A ROOT Color_t value.
 */
Color_t get_color_for_CDE_density(double log_density, double min_log_density, double max_log_density) {
    // Handle case where all densities are the same
    if (min_log_density >= max_log_density) {
        // Return a default color, e.g., blue
        return kBlue;
    }

    // Normalize log_density to [0, 1]
    double norm_val = (log_density - min_log_density) / (max_log_density - min_log_density);
    norm_val = std::max(0.0, std::min(1.0, norm_val)); // Clamp to [0, 1]

    // Gradient from Blue to Yellow
    // Blue: (0, 0, 255) or kBlue
    // Yellow: (255, 255, 0) or kYellow
    // Interpolate RGB values
    int r = static_cast<int>(0 + (255 - 0) * norm_val); // 0 -> 255
    int g = static_cast<int>(0 + (255 - 0) * norm_val); // 0 -> 255
    int b = static_cast<int>(255 + (0 - 255) * norm_val); // 255 -> 0

    // Ensure values are within 0-255
    r = std::max(0, std::min(255, r));
    g = std::max(0, std::min(255, g));
    b = std::max(0, std::min(255, b));

    Int_t color_index = TColor::GetColor(r, g, b);
    return color_index;
}