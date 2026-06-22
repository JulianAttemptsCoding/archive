#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <dirent.h>
#include <algorithm>
#include <sys/stat.h>
#include <cmath>
#include <regex>
#include "TApplication.h"
#include "TCanvas.h"
#include "TGraph.h"
#include "TMultiGraph.h"
#include "TLegend.h"
#include "TAxis.h"
#include "TStyle.h"
#include "TLatex.h"

using namespace std;

// Function to check if directory exists
bool directoryExists(const string& path) {
    struct stat info;
    if (stat(path.c_str(), &info) != 0) {
        return false;
    }
    return (info.st_mode & S_IFDIR);
}

// Function to extract all numbers from filename and create a sortable key
vector<int> extractNumbers(const string& filename) {
    vector<int> numbers;
    regex numberRegex("\\d+");
    auto numbers_begin = sregex_iterator(filename.begin(), filename.end(), numberRegex);
    auto numbers_end = sregex_iterator();
    
    for (sregex_iterator i = numbers_begin; i != numbers_end; ++i) {
        smatch match = *i;
        numbers.push_back(stoi(match.str()));
    }
    
    // If no numbers found, return a vector with a large number to sort them last
    if (numbers.empty()) {
        numbers.push_back(999999);
    }
    
    return numbers;
}

// Function to create title with only numbers from filename
string createTitle(const string& filename) {
    // Extract just the filename without path and extension
    string basename = filename;
    size_t lastSlash = basename.find_last_of("/");
    if (lastSlash != string::npos) {
        basename = basename.substr(lastSlash + 1);
    }
    size_t dotPos = basename.find_last_of(".");
    if (dotPos != string::npos) {
        basename = basename.substr(0, dotPos);
    }
    
    // Extract numbers
    vector<int> numbers = extractNumbers(basename);
    
    // Create title: "2025 + [the numbers in the file name]"
    string title = "2025";
    
    // Add all numbers
    for (size_t i = 0; i < numbers.size(); i++) {
        if (numbers[i] != 999999) { // Don't include the placeholder large number
            title += "-0718-14kv " + to_string(numbers[i]);
        }
    }
    
    return title;
}

// Function to create filename from title (remove spaces, replace with underscores)
string createFilenameFromTitle(const string& title) {
    string filename = title;
    // Replace spaces with underscores
    for (char& c : filename) {
        if (c == ' ') {
            c = '_';
        }
    }
    // Add pdf extension
    filename += ".pdf";
    return filename;
}

// Function to sort files by numeric values in their names
bool compareFiles(const string& a, const string& b) {
    // Extract just the filename without path
    string filename_a = a;
    string filename_b = b;
    
    size_t lastSlash_a = a.find_last_of("/");
    size_t lastSlash_b = b.find_last_of("/");
    
    if (lastSlash_a != string::npos) {
        filename_a = a.substr(lastSlash_a + 1);
    }
    if (lastSlash_b != string::npos) {
        filename_b = b.substr(lastSlash_b + 1);
    }
    
    vector<int> numbers_a = extractNumbers(filename_a);
    vector<int> numbers_b = extractNumbers(filename_b);
    
    // Compare number vectors lexicographically
    for (size_t i = 0; i < min(numbers_a.size(), numbers_b.size()); i++) {
        if (numbers_a[i] != numbers_b[i]) {
            return numbers_a[i] < numbers_b[i];
        }
    }
    
    // If one vector is shorter, it comes first
    return numbers_a.size() < numbers_b.size();
}

// Function to check if a value is valid (not inf or -inf)
bool isValidValue(double value) {
    return !isinf(value) && !isnan(value);
}

// Function to read CSV data
bool readCSVData(const string& filename, vector<double>& time, 
                 vector<vector<double>>& channels) {
    ifstream file(filename);
    if (!file.is_open()) {
        cerr << "Error opening file: " << filename << endl;
        return false;
    }

    string line;
    int lineNumber = 0;
    
    // Skip first 21 lines (rows 1-21)
    while (lineNumber < 21 && getline(file, line)) {
        lineNumber++;
    }
    
    // Clear vectors
    time.clear();
    for (auto& channel : channels) {
        channel.clear();
    }
    
    // Read data starting from row 22
    while (getline(file, line)) {
        if (line.empty()) continue;
        
        vector<string> values;
        string cell;
        bool inQuotes = false;
        
        // Parse CSV line
        for (size_t i = 0; i < line.length(); ++i) {
            if (line[i] == '"') {
                inQuotes = !inQuotes;
            } else if (line[i] == ',' && !inQuotes) {
                values.push_back(cell);
                cell.clear();
            } else {
                cell += line[i];
            }
        }
        values.push_back(cell);
        
        // Ensure we have enough columns
        if (values.size() >= 5) {
            try {
                double time_val = stod(values[0]);  // Column A - Time
                double trigger_val = stod(values[1]);  // Column B - Trigger
                double ch2_val = stod(values[2]);  // Column C - Ch2
                double ch3_val = stod(values[3]);  // Column D - Ch3
                double ch4_val = stod(values[4]);  // Column E - Ch4
                
                // Check if all values are valid (not inf or -inf)
                if (isValidValue(time_val) && isValidValue(trigger_val) && 
                    isValidValue(ch2_val) && isValidValue(ch3_val) && isValidValue(ch4_val)) {
                    time.push_back(time_val);
                    channels[0].push_back(trigger_val);
                    channels[1].push_back(ch2_val);
                    channels[2].push_back(ch3_val);
                    channels[3].push_back(ch4_val);
                }
            } catch (const exception& e) {
                // Skip lines with parsing errors
                continue;
            }
        }
    }
    
    file.close();
    cout << "Read " << time.size() << " valid data points from " << filename << endl;
    return true;
}

// Function to create and save plots for a file
void createPlots(const string& fullpath, const string& filename, 
                 const vector<double>& time, const vector<vector<double>>& channels) {
    // Create canvas with 4 subpads
    string canvasName = "canvas_" + filename;
    // Remove .csv extension and replace dots/invalid chars
    size_t dotPos = canvasName.find_last_of(".");
    if (dotPos != string::npos) {
        canvasName = canvasName.substr(0, dotPos);
    }
    
    // Replace invalid characters for ROOT canvas name
    for (char& c : canvasName) {
        if (!isalnum(c) && c != '_') {
            c = '_';
        }
    }
    
    TCanvas* canvas = new TCanvas(canvasName.c_str(), 
                                  ("Plots for " + filename).c_str(), 1200, 1200); // Increased height for title
    canvas->SetMargin(0.1, 0.1, 0.1, 0.15); // Adjust margins to accommodate title
    
    // Create title with only numbers
    string title = createTitle(filename);
    TLatex* latex = new TLatex();
    latex->SetTextSize(0.03);
    latex->SetTextAlign(22); // Center alignment
    latex->DrawLatexNDC(0.5, 0.96, title.c_str()); // Position at top center
    
    // Divide canvas for 4 subpads (leave space for title)
    TPad* pad1 = new TPad("pad1", "Trigger", 0.02, 0.52, 0.48, 0.92);
    TPad* pad2 = new TPad("pad2", "Ch2", 0.52, 0.52, 0.98, 0.92);
    TPad* pad3 = new TPad("pad3", "Ch3", 0.02, 0.08, 0.48, 0.48);
    TPad* pad4 = new TPad("pad4", "Ch4", 0.52, 0.08, 0.98, 0.48);
    
    pad1->Draw();
    pad2->Draw();
    pad3->Draw();
    pad4->Draw();
    
    vector<string> channelNames = {"Trigger", "Ch2", "Ch3", "Ch4"};
    vector<int> colors = {kBlue, kRed, kGreen+2, kMagenta};
    vector<TPad*> pads = {pad1, pad2, pad3, pad4};
    
    // Create separate plots for each channel
    for (int i = 0; i < 4; i++) {
        pads[i]->cd();
        
        if (time.size() > 0 && channels[i].size() == time.size()) {
            // Count valid points
            int validPoints = 0;
            for (size_t j = 0; j < time.size(); j++) {
                if (isValidValue(time[j]) && isValidValue(channels[i][j])) {
                    validPoints++;
                }
            }
            
            if (validPoints > 0) {
                TGraph* graph = new TGraph(validPoints);
                int pointIndex = 0;
                for (size_t j = 0; j < time.size(); j++) {
                    if (isValidValue(time[j]) && isValidValue(channels[i][j])) {
                        graph->SetPoint(pointIndex, time[j], channels[i][j]);
                        pointIndex++;
                    }
                }
                
                graph->SetTitle((channelNames[i] + " vs Time").c_str());
                graph->GetXaxis()->SetTitle("Time (sec)");
                graph->GetYaxis()->SetTitle(channelNames[i].c_str());
                graph->SetLineColor(colors[i]);
                graph->SetLineWidth(2);
                graph->Draw("AL");
            } else {
                cout << "No valid data points for " << channelNames[i] << " in " << filename << endl;
            }
        } else {
            cout << "No valid data for " << channelNames[i] << " in " << filename << endl;
        }
    }
    
    // Create PDF filename from title
    string outputName = createFilenameFromTitle(title);
    
    canvas->SaveAs(outputName.c_str());
    cout << "Saved plots to: " << outputName << endl;
    cout << "Title: " << title << endl;
    
    delete latex;
    delete canvas;
}

int main() {
    // Try common shared directory paths
    vector<string> possiblePaths = {
        "/shared",
        "/home/shared",
        "./shared",
        "../shared",
        "/mnt/shared",
        "."
    };
    
    string folderPath = "";
    
    // Find the first existing directory
    for (const string& path : possiblePaths) {
        if (directoryExists(path)) {
            folderPath = path;
            break;
        }
    }
    
    // If no shared directory found, use current directory
    if (folderPath.empty()) {
        folderPath = ".";
        cout << "Using current directory as no shared folder found" << endl;
    } else {
        cout << "Found shared directory: " << folderPath << endl;
    }
    
    // Initialize ROOT application
    int argc = 0;
    char** argv = nullptr;
    TApplication app("app", &argc, argv);
    
    // Set ROOT style
    gStyle->SetOptStat(0);
    
    // Get list of CSV files from the directory
    vector<string> csvFiles;
    
    DIR* dir = opendir(folderPath.c_str());
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            string filename = entry->d_name;
            // Check if file ends with .csv
            if (filename.length() >= 4 && 
                filename.substr(filename.length() - 4) == ".csv") {
                string fullPath = (folderPath == "." ? "" : folderPath) + 
                                (folderPath == "." ? "" : "/") + filename;
                csvFiles.push_back(fullPath);
            }
        }
        closedir(dir);
    } else {
        cerr << "Error opening directory: " << folderPath << endl;
        cerr << "Current working directory: ";
        system("pwd");
        cerr << "Directory contents: ";
        system("ls -la");
        return 1;
    }
    
    if (csvFiles.empty()) {
        cout << "No CSV files found in " << folderPath << endl;
        system(("ls -la " + folderPath).c_str());
        return 0;
    }
    
    // Sort files by numeric values in their names
    sort(csvFiles.begin(), csvFiles.end(), compareFiles);
    
    cout << "Found " << csvFiles.size() << " CSV files (sorted by numeric values):" << endl;
    for (size_t i = 0; i < min(csvFiles.size(), size_t(15)); i++) {
        // Extract just the filename for display
        size_t lastSlash = csvFiles[i].find_last_of("/");
        string displayFilename = (lastSlash != string::npos) ? 
                               csvFiles[i].substr(lastSlash + 1) : csvFiles[i];
        cout << "  " << (i + 1) << ". " << displayFilename << endl;
    }
    if (csvFiles.size() > 15) {
        cout << "  ... and " << (csvFiles.size() - 15) << " more files" << endl;
    }
    
    // Process first 10 files (or fewer if less than 10 exist)
    int filesToProcess = min(10, (int)csvFiles.size());
    cout << "\nProcessing " << filesToProcess << " CSV files..." << endl;
    
    for (int i = 0; i < filesToProcess; i++) {
        // Extract just the filename for display
        size_t lastSlash = csvFiles[i].find_last_of("/");
        string displayFilename = (lastSlash != string::npos) ? 
                               csvFiles[i].substr(lastSlash + 1) : csvFiles[i];
        
        cout << "\nProcessing file " << (i + 1) << ": " << displayFilename << endl;
        
        vector<double> time;
        vector<vector<double>> channels(4);  // B, C, D, E columns
        
        if (readCSVData(csvFiles[i], time, channels)) {
            if (time.size() > 0) {
                createPlots(csvFiles[i], displayFilename, time, channels);
                cout << "Successfully generated plots for " << displayFilename << endl;
            } else {
                cout << "No valid data found in " << displayFilename << endl;
            }
        } else {
            cerr << "Failed to process " << displayFilename << endl;
        }
    }
    
    cout << "\nAll plots generated successfully!" << endl;
    
    return 0;
}