//
// File: filter_and_plot_csv.C
//
// Description:
// This ROOT C++ macro processes multiple folders containing CSV files,
// creates filtered histograms with logarithmic y-axis for each folder,
// and saves them with descriptive names. Can process current directory
// or scan subdirectories automatically.
//
// Dependencies:
// This script uses standard C++ file I/O for CSV parsing.
// No external library like libxls is required for CSV.
//
// How to Run in ROOT:
// 1. Launch the ROOT interactive terminal:
//    root
//
// 2. For current directory only:
//    .L filter_and_plot_csv.C+
//    process_csv_files()
//
// 3. For all subdirectories:
//    .L filter_and_plot_csv.C+
//    process_all_folders()
//
// 4. For specific folders:
//    .L filter_and_plot_csv.C+
//    process_specific_folders()
//
// The '+' after the macro name tells ROOT to compile it using ACLiC.
//

#include <TSystem.h>
#include <TString.h>
#include <TGraph.h>
#include <TH1F.h>
#include <TCanvas.h>
#include <TStyle.h>
#include <TAxis.h>
#include <TPaveText.h>
#include <TLegend.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <numeric>
#include <fstream>   // For CSV file reading
#include <string>    // For string manipulation
#include <sstream>   // For parsing lines
#include <algorithm> // For min_element, max_element
#include <limits>    // For numeric limits

// --- Struct to hold folder processing results ---
struct FolderResult {
    std::string folder_name;
    std::string folder_path;
    int total_files;
    int valid_data_points;
    int filtered_data_points;
    double mean;
    double stddev;
    double min_val;
    double max_val;
    bool success;
    std::string error_message;
};

// --- Function Declarations ---

// Main functions
void process_csv_files();                    // Process current directory only
void process_all_folders();                 // Process all subdirectories
void process_specific_folders();            // Process user-specified folders
void create_summary_report(const std::vector<FolderResult>& results);

// Core processing functions
bool process_single_folder(const std::string& folder_path, const std::string& folder_name, FolderResult& result);
bool read_csv_data(const char* filename, std::vector<double>& time, std::vector<double>& amplitude);
void calculate_stats(const std::vector<double>& data, double& mean, double& stddev);

// Utility functions
bool is_valid_number(double value);
std::string trim(const std::string& str);
bool safe_string_to_double(const std::string& str, double& result);
std::string sanitize_filename(const std::string& input);
std::vector<std::string> get_subdirectories(const std::string& path);

// --- Main Processing Functions ---

/**
 * @brief Process CSV files in the current directory only
 */
void process_csv_files() {
    std::cout << "=== PROCESSING CURRENT DIRECTORY ONLY ===" << std::endl;
    
    FolderResult result;
    TString current_dir = gSystem->pwd();
    std::string current_path = current_dir.Data();
    
    // Debug: Print current directory info
    std::cout << "Current directory: " << current_path << std::endl;
    std::cout << "Directory accessible: " << (gSystem->AccessPathName(current_path.c_str()) ? "NO" : "YES") << std::endl;
    
    // Extract just the folder name from the path
    size_t last_slash = current_path.find_last_of("/\\");
    std::string folder_name = (last_slash != std::string::npos) ? 
                              current_path.substr(last_slash + 1) : current_path;
    
    std::cout << "Folder name: " << folder_name << std::endl;
    
    if (process_single_folder(current_path, folder_name, result)) {
        std::cout << "Successfully processed current directory: " << folder_name << std::endl;
    } else {
        std::cerr << "Failed to process current directory: " << result.error_message << std::endl;
    }
}

/**
 * @brief Process all subdirectories in the current directory
 */
void process_all_folders() {
    std::cout << "=== PROCESSING ALL SUBDIRECTORIES ===" << std::endl;
    
    TString current_dir = gSystem->pwd();
    std::string base_path = current_dir.Data();
    
    std::vector<std::string> subdirs = get_subdirectories(base_path);
    
    if (subdirs.empty()) {
        std::cout << "No subdirectories found. Processing current directory instead." << std::endl;
        process_csv_files();
        return;
    }
    
    std::cout << "Found " << subdirs.size() << " subdirectories to process:" << std::endl;
    for (size_t i = 0; i < subdirs.size(); ++i) {
        std::cout << "  - " << subdirs[i] << std::endl;
    }
    std::cout << std::endl;
    
    std::vector<FolderResult> results;
    
    for (size_t i = 0; i < subdirs.size(); ++i) {
        const std::string& subdir = subdirs[i];
        std::cout << "Processing folder: " << subdir << std::endl;
        std::cout << "----------------------------------------" << std::endl;
        
        FolderResult result;
        std::string full_path = base_path + "/" + subdir;
        
        if (process_single_folder(full_path, subdir, result)) {
            std::cout << "✓ Successfully processed: " << subdir << std::endl;
        } else {
            std::cerr << "✗ Failed to process: " << subdir << " - " << result.error_message << std::endl;
        }
        
        results.push_back(result);
        std::cout << std::endl;
    }
    
    // Create summary report
    create_summary_report(results);
}

/**
 * @brief Process specific folders (user can modify this function)
 */
void process_specific_folders() {
    std::cout << "=== PROCESSING SPECIFIC FOLDERS ===" << std::endl;
    
    // User can modify this list to specify which folders to process
    std::vector<std::string> folders_to_process;
    folders_to_process.push_back("20250716-14kv");
    folders_to_process.push_back("20250716-16kv");
    folders_to_process.push_back("20250716-18kv");
    // Add more folder names as needed
    
    TString current_dir = gSystem->pwd();
    std::string base_path = current_dir.Data();
    
    std::vector<FolderResult> results;
    
    for (size_t i = 0; i < folders_to_process.size(); ++i) {
        const std::string& folder = folders_to_process[i];
        std::cout << "Processing folder: " << folder << std::endl;
        std::cout << "----------------------------------------" << std::endl;
        
        FolderResult result;
        std::string full_path = base_path + "/" + folder;
        
        // Check if folder exists
        if (gSystem->AccessPathName(full_path.c_str())) {
            std::cerr << "✗ Folder not found: " << folder << std::endl;
            result.folder_name = folder;
            result.folder_path = full_path;
            result.success = false;
            result.error_message = "Folder not found";
            results.push_back(result);
            continue;
        }
        
        if (process_single_folder(full_path, folder, result)) {
            std::cout << "✓ Successfully processed: " << folder << std::endl;
        } else {
            std::cerr << "✗ Failed to process: " << folder << " - " << result.error_message << std::endl;
        }
        
        results.push_back(result);
        std::cout << std::endl;
    }
    
    // Create summary report
    create_summary_report(results);
}

/**
 * @brief Process a single folder and create histogram
 */
bool process_single_folder(const std::string& folder_path, const std::string& folder_name, FolderResult& result) {
    // Initialize result
    result.folder_name = folder_name;
    result.folder_path = folder_path;
    result.total_files = 0;
    result.valid_data_points = 0;
    result.filtered_data_points = 0;
    result.success = false;
    
    // Debug: Print folder processing info
    std::cout << "Processing folder: " << folder_path << std::endl;
    std::cout << "Folder exists: " << (gSystem->AccessPathName(folder_path.c_str()) ? "NO" : "YES") << std::endl;
    
    // Change to the target directory
    TString original_dir = gSystem->pwd();
    std::cout << "Original directory: " << original_dir.Data() << std::endl;
    
    int cd_result = gSystem->cd(folder_path.c_str());
    std::cout << "cd result: " << cd_result << std::endl;
    
    if (cd_result != 0) {
        result.error_message = "Cannot access folder: " + folder_path + " (cd returned: " + std::to_string(cd_result) + ")";
        return false;
    }
    
    // Verify we're in the right directory
    TString new_dir = gSystem->pwd();
    std::cout << "New directory after cd: " << new_dir.Data() << std::endl;
    
    std::vector<double> all_time;
    std::vector<double> all_amplitude;
    
    // Process CSV files in this directory
    void* dir_handle = gSystem->OpenDirectory(".");
    if (!dir_handle) {
        result.error_message = "Cannot open directory for reading";
        gSystem->cd(original_dir);
        return false;
    }
    
    std::cout << "Directory contents:" << std::endl;
    const char* entry;
    while ((entry = gSystem->GetDirEntry(dir_handle))) {
        TString filename(entry);
        std::cout << "  " << filename.Data() << std::endl;
        if (filename.EndsWith(".csv")) {
            std::cout << "    -> Processing CSV file: " << filename.Data() << std::endl;
            result.total_files++;
            std::vector<double> file_time, file_amplitude;
            if (read_csv_data(filename.Data(), file_time, file_amplitude)) {
                all_time.insert(all_time.end(), file_time.begin(), file_time.end());
                all_amplitude.insert(all_amplitude.end(), file_amplitude.begin(), file_amplitude.end());
                result.valid_data_points += file_amplitude.size();
                std::cout << "    -> Found " << file_amplitude.size() << " data points" << std::endl;
            } else {
                std::cout << "    -> Failed to read CSV file" << std::endl;
            }
        }
    }
    gSystem->FreeDirectory(dir_handle);
    
    std::cout << "Total CSV files found: " << result.total_files << std::endl;
    std::cout << "Total data points: " << result.valid_data_points << std::endl;
    
    if (all_amplitude.empty()) {
        result.error_message = "No valid data found in CSV files";
        gSystem->cd(original_dir);
        return false;
    }
    
    // Calculate statistics
    calculate_stats(all_amplitude, result.mean, result.stddev);
    
    if (!is_valid_number(result.mean) || !is_valid_number(result.stddev)) {
        result.error_message = "Invalid statistical calculations";
        gSystem->cd(original_dir);
        return false;
    }
    
    if (result.stddev == 0.0) {
        result.error_message = "Standard deviation is zero - all data points identical";
        gSystem->cd(original_dir);
        return false;
    }
    
    // Store domain bounds
    result.min_val = *std::min_element(all_amplitude.begin(), all_amplitude.end());
    result.max_val = *std::max_element(all_amplitude.begin(), all_amplitude.end());
    
    // Apply 3-sigma filter
    double lower_bound = result.mean - 3 * result.stddev;
    double upper_bound = result.mean + 3 * result.stddev;
    
    std::vector<double> filtered_amplitude;
    for (size_t i = 0; i < all_amplitude.size(); ++i) {
        const double& amp = all_amplitude[i];
        if (amp < lower_bound || amp > upper_bound) {
            filtered_amplitude.push_back(amp);
        }
    }
    
    result.filtered_data_points = filtered_amplitude.size();
    
    if (filtered_amplitude.empty()) {
        result.error_message = "All data removed by 3-sigma filter";
        gSystem->cd(original_dir);
        return false;
    }
    
    // Create histogram
    gStyle->SetOptStat(1111);
    
    std::string canvas_name = "canvas_" + sanitize_filename(folder_name);
    std::string canvas_title = "Filtered Amplitude: " + folder_name;
    
    TCanvas* canvas = new TCanvas(canvas_name.c_str(), canvas_title.c_str(), 800, 600);
    canvas->SetLogy();
    
    // Add padding to domain
    double domain_padding = (result.max_val - result.min_val) * 0.05;
    double hist_min = result.min_val - domain_padding;
    double hist_max = result.max_val + domain_padding;
    
    std::string hist_name = "hist_" + sanitize_filename(folder_name);
    std::string hist_title = "Filtered Amplitude: " + folder_name + ";Amplitude;Counts";
    
    TH1F* hist = new TH1F(hist_name.c_str(), hist_title.c_str(), 100, hist_min, hist_max);
    
    for (size_t i = 0; i < filtered_amplitude.size(); ++i) {
        const double& amp = filtered_amplitude[i];
        hist->Fill(amp);
    }
    
    hist->SetFillColor(kBlue - 9);
    hist->SetLineColor(kBlue);
    hist->SetMinimum(0.1);
    
    // Add folder information to the plot
    TPaveText* info = new TPaveText(0.6, 0.7, 0.9, 0.9, "NDC");
    info->SetFillColor(0);
    info->SetTextAlign(12);
    info->AddText(("Folder: " + folder_name).c_str());
    info->AddText(("Files: " + std::to_string(result.total_files)).c_str());
    info->AddText(("Points: " + std::to_string(result.valid_data_points)).c_str());
    info->AddText(("Filtered: " + std::to_string(result.filtered_data_points)).c_str());
    
    hist->Draw();
    info->Draw();
    
    // Save with descriptive filename
    std::string output_filename = "histogram_" + sanitize_filename(folder_name) + ".png";
    canvas->SaveAs(output_filename.c_str());
    
    std::cout << "Histogram saved as: " << output_filename << std::endl;
    std::cout << "Files processed: " << result.total_files << std::endl;
    std::cout << "Valid data points: " << result.valid_data_points << std::endl;
    std::cout << "Filtered data points: " << result.filtered_data_points << std::endl;
    
    // Return to original directory
    gSystem->cd(original_dir);
    
    result.success = true;
    return true;
}

/**
 * @brief Create a summary report of all processed folders
 */
void create_summary_report(const std::vector<FolderResult>& results) {
    std::cout << "\n=== PROCESSING SUMMARY REPORT ===" << std::endl;
    std::cout << "Total folders processed: " << results.size() << std::endl;
    
    int successful = 0;
    int failed = 0;
    
    for (size_t i = 0; i < results.size(); ++i) {
        const FolderResult& result = results[i];
        if (result.success) {
            successful++;
        } else {
            failed++;
        }
    }
    
    std::cout << "Successful: " << successful << std::endl;
    std::cout << "Failed: " << failed << std::endl;
    std::cout << std::endl;
    
    // Detailed results
    std::cout << "Detailed Results:" << std::endl;
    std::cout << "=================" << std::endl;
    
    for (size_t i = 0; i < results.size(); ++i) {
        const FolderResult& result = results[i];
        std::cout << "Folder: " << result.folder_name << std::endl;
        if (result.success) {
            std::cout << "  Status: ✓ SUCCESS" << std::endl;
            std::cout << "  Files: " << result.total_files << std::endl;
            std::cout << "  Data points: " << result.valid_data_points << std::endl;
            std::cout << "  Filtered points: " << result.filtered_data_points << std::endl;
            std::cout << "  Mean: " << result.mean << std::endl;
            std::cout << "  Std Dev: " << result.stddev << std::endl;
            std::cout << "  Range: [" << result.min_val << ", " << result.max_val << "]" << std::endl;
        } else {
            std::cout << "  Status: ✗ FAILED" << std::endl;
            std::cout << "  Error: " << result.error_message << std::endl;
        }
        std::cout << std::endl;
    }
    
    // Save summary to file
    std::ofstream summary_file("processing_summary.txt");
    if (summary_file.is_open()) {
        summary_file << "Processing Summary Report\n";
        summary_file << "========================\n\n";
        summary_file << "Total folders: " << results.size() << "\n";
        summary_file << "Successful: " << successful << "\n";
        summary_file << "Failed: " << failed << "\n\n";
        
        for (size_t i = 0; i < results.size(); ++i) {
            const FolderResult& result = results[i];
            summary_file << "Folder: " << result.folder_name << "\n";
            if (result.success) {
                summary_file << "  Status: SUCCESS\n";
                summary_file << "  Files: " << result.total_files << "\n";
                summary_file << "  Data points: " << result.valid_data_points << "\n";
                summary_file << "  Filtered points: " << result.filtered_data_points << "\n";
                summary_file << "  Mean: " << result.mean << "\n";
                summary_file << "  Std Dev: " << result.stddev << "\n";
                summary_file << "  Range: [" << result.min_val << ", " << result.max_val << "]\n";
            } else {
                summary_file << "  Status: FAILED\n";
                summary_file << "  Error: " << result.error_message << "\n";
            }
            summary_file << "\n";
        }
        summary_file.close();
        std::cout << "Summary report saved to: processing_summary.txt" << std::endl;
    }
}

// --- Utility Functions ---

/**
 * @brief Get list of subdirectories in a given path
 */
std::vector<std::string> get_subdirectories(const std::string& path) {
    std::vector<std::string> subdirs;
    
    void* dir_handle = gSystem->OpenDirectory(path.c_str());
    if (!dir_handle) {
        return subdirs;
    }
    
    const char* entry;
    while ((entry = gSystem->GetDirEntry(dir_handle))) {
        std::string entry_name(entry);
        if (entry_name != "." && entry_name != "..") {
            std::string full_path = path + "/" + entry_name;
            Long_t id, size, flags, modtime;
            if (gSystem->GetPathInfo(full_path.c_str(), &id, &size, &flags, &modtime) == 0) {
                if (flags & 2) { // Check if it's a directory
                    subdirs.push_back(entry_name);
                }
            }
        }
    }
    gSystem->FreeDirectory(dir_handle);
    
    std::sort(subdirs.begin(), subdirs.end());
    return subdirs;
}

/**
 * @brief Sanitize filename by removing/replacing problematic characters
 */
std::string sanitize_filename(const std::string& input) {
    std::string result = input;
    
    // Replace problematic characters
    std::replace(result.begin(), result.end(), '/', '_');
    std::replace(result.begin(), result.end(), '\\', '_');
    std::replace(result.begin(), result.end(), ':', '_');
    std::replace(result.begin(), result.end(), '*', '_');
    std::replace(result.begin(), result.end(), '?', '_');
    std::replace(result.begin(), result.end(), '"', '_');
    std::replace(result.begin(), result.end(), '<', '_');
    std::replace(result.begin(), result.end(), '>', '_');
    std::replace(result.begin(), result.end(), '|', '_');
    std::replace(result.begin(), result.end(), ' ', '_');
    
    return result;
}

// --- Helper functions ---

bool read_csv_data(const char* filename, std::vector<double>& time, std::vector<double>& amplitude) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        return false;
    }

    std::string line;
    int current_line_num = 0;
    const int DATA_START_ROW = 21;
    int valid_rows = 0;

    while (std::getline(file, line)) {
        current_line_num++;
        
        if (current_line_num <= DATA_START_ROW) {
            continue;
        }

        if (trim(line).empty()) {
            continue;
        }

        std::stringstream ss(line);
        std::string segment;
        std::vector<double> row_values;

        while(std::getline(ss, segment, ',')) {
            double value;
            if (safe_string_to_double(trim(segment), value)) {
                row_values.push_back(value);
            } else {
                row_values.push_back(0.0);
            }
        }

        if (row_values.size() >= 5) {
            double current_time = row_values[0];
            double current_amplitude_sum = row_values[2] + row_values[3] + row_values[4];

            if (is_valid_number(current_time) && is_valid_number(current_amplitude_sum)) {
                time.push_back(current_time);
                amplitude.push_back(current_amplitude_sum);
                valid_rows++;
            }
        }
    }

    file.close();
    return valid_rows > 0;
}

void calculate_stats(const std::vector<double>& data, double& mean, double& stddev) {
    if (data.empty()) {
        mean = 0.0;
        stddev = 0.0;
        return;
    }

    double sum = 0.0;
    for (size_t i = 0; i < data.size(); ++i) {
        sum += data[i];
    }
    mean = sum / data.size();

    double sq_sum = 0.0;
    for (size_t i = 0; i < data.size(); ++i) {
        double diff = data[i] - mean;
        sq_sum += diff * diff;
    }
    stddev = std::sqrt(sq_sum / data.size());
}

bool is_valid_number(double value) {
    return std::isfinite(value) && !std::isnan(value);
}

std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "";
    }
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

bool safe_string_to_double(const std::string& str, double& result) {
    if (str.empty()) {
        return false;
    }
    
    try {
        size_t processed = 0;
        result = std::stod(str, &processed);
        
        if (processed != str.length()) {
            return false;
        }
        
        return is_valid_number(result);
        
    } catch (const std::invalid_argument&) {
        return false;
    } catch (const std::out_of_range&) {
        return false;
    }
}