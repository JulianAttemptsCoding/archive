//
// File: filter_and_plot_csv.C
//
// Description:
// This ROOT C++ macro scans the current directory for CSV files (.csv),
// reads specific columns, filters the data by removing points
// within 3 standard deviations of the mean, and then plots the result
// as a histogram with logarithmic y-axis and full domain x-axis.
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

// --- Function Declarations ---

// Main function to be called from ROOT
void process_csv_files();

// Helper function to read data from a single CSV file
// This function assumes:
// - Time in the first column (index 0)
// - Amplitude components in columns 2, 3, 4 (0-indexed C, D, E)
// - Data starts from a specific row (e.g., row 22 in Excel, which is line 21 if 0-indexed and no header)
bool read_csv_data(const char* filename, std::vector<double>& time, std::vector<double>& amplitude);

// Helper function to calculate statistics (mean and standard deviation)
void calculate_stats(const std::vector<double>& data, double& mean, double& stddev);


// --- Main Processing Function ---

void process_csv_files() {
    std::cout << "Starting CSV file processing..." << std::endl;

    // Vectors to store the aggregated data from all files
    std::vector<double> all_time;
    std::vector<double> all_amplitude;

    // Get the current directory
    TString current_dir = gSystem->pwd();
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
            std::cout << "Found CSV file: " << filename << std::endl;
            if (!read_csv_data(filename.Data(), all_time, all_amplitude)) {
                std::cerr << "Warning: Failed to read data from " << filename << std::endl;
            }
        }
    }
    gSystem->FreeDirectory(dir_handle);

    if (all_amplitude.empty()) {
        std::cerr << "Error: No data was successfully read from any CSV files. Aborting." << std::endl;
        return;
    }

    std::cout << "Total data points read: " << all_amplitude.size() << std::endl;

    // --- Store the full input domain bounds ---
    double input_min = *std::min_element(all_amplitude.begin(), all_amplitude.end());
    double input_max = *std::max_element(all_amplitude.begin(), all_amplitude.end());
    std::cout << "Full input domain: [" << input_min << ", " << input_max << "]" << std::endl;

    // --- 3-Sigma Filtering ---
    double mean, stddev;
    calculate_stats(all_amplitude, mean, stddev);
    std::cout << "Calculated Mean: " << mean << ", StdDev (sigma): " << stddev << std::endl;

    double lower_bound = mean - 3 * stddev;
    double upper_bound = mean + 3 * stddev;
    std::cout << "3-Sigma filter range (data to be removed): [" << lower_bound << ", " << upper_bound << "]" << std::endl;

    std::vector<double> filtered_amplitude;
    for (const auto& amp : all_amplitude) {
        // Keep data points that are OUTSIDE the 3-sigma range
        if (amp < lower_bound || amp > upper_bound) {
            filtered_amplitude.push_back(amp);
        }
    }

    std::cout << "Data points remaining after 3-sigma filter: " << filtered_amplitude.size() << std::endl;

    if (filtered_amplitude.empty()) {
        std::cerr << "Error: All data was removed by the 3-sigma filter. Cannot create histogram." << std::endl;
        return;
    }

    // --- Histogram Creation and Plotting ---
    gStyle->SetOptStat(1111); // Show statistics on the plot

    TCanvas* canvas = new TCanvas("c1", "Filtered Amplitude Histogram", 800, 600);
    
    // Set logarithmic y-axis
    canvas->SetLogy();
    
    // Use the full input domain for x-axis bounds, not just the filtered data
    TH1F* hist = new TH1F("hist_amplitude", "Filtered Amplitude Distribution;Amplitude;Counts", 100, input_min, input_max);

    for (const auto& amp : filtered_amplitude) {
        hist->Fill(amp);
    }

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
    std::cout << "X-axis covers full input domain: [" << input_min << ", " << input_max << "]" << std::endl;
    std::cout << "Y-axis is now logarithmic" << std::endl;
}

// --- Helper Function Implementations ---

/**
 * @brief Reads data from a specified .csv file.
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

    while (std::getline(file, line)) {
        if (current_line_num < DATA_START_ROW) {
            current_line_num++;
            continue; // Skip lines until the data start row
        }

        std::stringstream ss(line);
        std::string segment;
        std::vector<double> row_values;

        // Parse each segment (column) separated by comma
        while(std::getline(ss, segment, ',')) {
            try {
                row_values.push_back(std::stod(segment));
            } catch (const std::invalid_argument& e) {
                // Handle non-numeric data, e.g., empty cells or text
                row_values.push_back(0.0); // Or some other placeholder/error handling
            } catch (const std::out_of_range& e) {
                row_values.push_back(0.0); // Value too large/small
            }
        }

        // Ensure we have enough columns before accessing
        if (row_values.size() >= 5) { // Need at least 5 columns (A, B, C, D, E)
            double current_time = row_values[0]; // Column A (0-indexed)
            double current_amplitude_sum = row_values[2] + row_values[3] + row_values[4]; // Columns C, D, E

            time.push_back(current_time);
            amplitude.push_back(current_amplitude_sum);
        } else {
            // std::cerr << "Warning: Skipping row " << current_line_num + 1 << " in " << filename << " due to insufficient columns." << std::endl;
        }
        current_line_num++;
    }

    file.close();
    return true;
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

    double sum = std::accumulate(data.begin(), data.end(), 0.0);
    mean = sum / data.size();

    double sq_sum = 0.0;
    for (const auto& x : data) {
        sq_sum += (x - mean) * (x - mean);
    }
    stddev = std::sqrt(sq_sum / data.size());
}