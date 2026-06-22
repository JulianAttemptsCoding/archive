#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <map>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <dirent.h>
#include <sys/stat.h>
#include <sstream>

// Structure to hold data for each voltage category
struct VoltageData {
    std::vector<std::vector<double>> allData; // All files' data for this category
    std::vector<double> averages;             // Average values
    std::vector<double> q1, q3;              // IQR quartiles
    int voltage;                              // Voltage value (8-14 kV)
};

class CSVAnalyzer {
private:
    std::map<int, VoltageData> voltageCategories;
    
    // Extract 3rd digit from right of filename
    int getThirdDigit(const std::string& filename) {
        // Remove .csv extension
        std::string name = filename;
        if (name.length() >= 4 && name.substr(name.length()-4) == ".csv") {
            name = name.substr(0, name.length()-4);
        }
        
        // Extract digits only
        std::string digits;
        for (char c : name) {
            if (std::isdigit(c)) {
                digits += c;
            }
        }
        
        if (digits.length() >= 3) {
            return digits[digits.length()-3] - '0';
        } else if (digits.length() == 2) {
            return 0; // 3rd digit is 0 if only 2 digits
        } else {
            return -1; // Invalid
        }
    }
    
    // Parse CSV and extract data from rows 22-98, columns C-E
    std::vector<std::vector<double>> parseCSV(const std::string& filepath) {
        std::vector<std::vector<double>> data;
        std::ifstream file(filepath);
        std::string line;
        int rowNum = 0;
        
        if (!file.is_open()) {
            std::cerr << "Error opening file: " << filepath << std::endl;
            return data;
        }
        
        while (std::getline(file, line) && rowNum < 98) {
            rowNum++;
            
            // Skip rows before 22
            if (rowNum < 22) continue;
            
            std::vector<double> rowData;
            std::stringstream ss(line);
            std::string cell;
            int colNum = 0;
            
            while (std::getline(ss, cell, ',') && colNum < 5) {
                colNum++;
                
                // Extract columns C, D, E (columns 3, 4, 5)
                if (colNum >= 3 && colNum <= 5) {
                    try {
                        double value = std::stod(cell);
                        rowData.push_back(value);
                    } catch (const std::exception& e) {
                        // Handle non-numeric data
                        rowData.push_back(0.0);
                    }
                }
            }
            
            if (rowData.size() == 3) {
                data.push_back(rowData);
            }
        }
        
        file.close();
        return data;
    }
    
    // Calculate quartiles for IQR
    void calculateQuartiles(std::vector<double>& values, double& q1, double& q3) {
        if (values.empty()) {
            q1 = q3 = 0.0;
            return;
        }
        
        std::sort(values.begin(), values.end());
        size_t n = values.size();
        
        if (n % 4 == 0) {
            q1 = (values[n/4 - 1] + values[n/4]) / 2.0;
            q3 = (values[3*n/4 - 1] + values[3*n/4]) / 2.0;
        } else {
            q1 = values[n/4];
            q3 = values[3*n/4];
        }
    }
    
    // Generate gnuplot script for visualization
    void generatePlot() {
        std::ofstream plotScript("plot_data.gnu");
        std::ofstream dataFile("averaged_data.dat");
        std::ofstream iqrFile("iqr_data.dat");
        
        // Write data files
        dataFile << "# Voltage Column_C Column_D Column_E\n";
        iqrFile << "# Voltage Q1_C Q3_C Q1_D Q3_D Q1_E Q3_E\n";
        
        for (auto& pair : voltageCategories) {
            int voltage = pair.first;
            VoltageData& data = pair.second;
            
            if (!data.averages.empty()) {
                dataFile << voltage + 8 << " " 
                        << data.averages[0] << " " 
                        << data.averages[1] << " " 
                        << data.averages[2] << "\n";
                
                if (data.q1.size() == 3 && data.q3.size() == 3) {
                    iqrFile << voltage + 8 << " "
                           << data.q1[0] << " " << data.q3[0] << " "
                           << data.q1[1] << " " << data.q3[1] << " "
                           << data.q1[2] << " " << data.q3[2] << "\n";
                }
            }
        }
        
        dataFile.close();
        iqrFile.close();
        
        // Generate gnuplot script
        plotScript << R"(
set terminal png size 1200,800
set output 'voltage_analysis.png'
set title 'CERN Voltage Analysis with IQR'
set xlabel 'Voltage (kV)'
set ylabel 'Average Values'
set grid
set key top left

# Plot averages with lines
plot 'averaged_data.dat' using 1:2 with linespoints title 'Column C' lw 2 pt 7 ps 1.5, \
     'averaged_data.dat' using 1:3 with linespoints title 'Column D' lw 2 pt 9 ps 1.5, \
     'averaged_data.dat' using 1:4 with linespoints title 'Column E' lw 2 pt 11 ps 1.5, \
     'iqr_data.dat' using 1:2 with lines title 'Q1 Column C' lt 1 lw 1 dt 2, \
     'iqr_data.dat' using 1:3 with lines title 'Q3 Column C' lt 1 lw 1 dt 2, \
     'iqr_data.dat' using 1:4 with lines title 'Q1 Column D' lt 2 lw 1 dt 2, \
     'iqr_data.dat' using 1:5 with lines title 'Q3 Column D' lt 2 lw 1 dt 2, \
     'iqr_data.dat' using 1:6 with lines title 'Q1 Column E' lt 3 lw 1 dt 2, \
     'iqr_data.dat' using 1:7 with lines title 'Q3 Column E' lt 3 lw 1 dt 2
)";
        
        plotScript.close();
        
        // Execute gnuplot
        system("gnuplot plot_data.gnu");
        std::cout << "Plot generated: voltage_analysis.png\n";
    }
    
public:
    void processDirectory(const std::string& dirPath = ".") {
        DIR* dir = opendir(dirPath.c_str());
        if (!dir) {
            std::cerr << "Error opening directory: " << dirPath << std::endl;
            return;
        }
        
        struct dirent* entry;
        std::vector<std::string> csvFiles;
        
        // Find all CSV files
        while ((entry = readdir(dir)) != nullptr) {
            std::string filename = entry->d_name;
            if (filename.length() > 4 && 
                filename.substr(filename.length()-4) == ".csv") {
                csvFiles.push_back(filename);
            }
        }
        closedir(dir);
        
        std::cout << "Found " << csvFiles.size() << " CSV files\n";
        
        // Process each CSV file
        for (const std::string& filename : csvFiles) {
            int thirdDigit = getThirdDigit(filename);
            
            if (thirdDigit >= 0 && thirdDigit <= 6) { // 0-6 maps to 8-14kV
                std::cout << "Processing " << filename 
                         << " (3rd digit: " << thirdDigit 
                         << ", Voltage: " << (thirdDigit + 8) << "kV)\n";
                
                std::vector<std::vector<double>> fileData = parseCSV(dirPath + "/" + filename);
                
                if (!fileData.empty()) {
                    voltageCategories[thirdDigit].voltage = thirdDigit + 8;
                    voltageCategories[thirdDigit].allData.push_back(
                        std::vector<double>(fileData.size() * 3)); // Flatten for processing
                    
                    // Flatten the data for easier processing
                    auto& lastDataSet = voltageCategories[thirdDigit].allData.back();
                    int idx = 0;
                    for (const auto& row : fileData) {
                        for (double val : row) {
                            if (idx < lastDataSet.size()) {
                                lastDataSet[idx++] = val;
                            }
                        }
                    }
                }
            }
        }
        
        // Calculate averages and IQR for each category
        calculateStatistics();
        
        // Generate plot
        generatePlot();
        
        // Print summary
        printSummary();
    }
    
    void calculateStatistics() {
        for (auto& pair : voltageCategories) {
            VoltageData& data = pair.second;
            
            if (data.allData.empty()) continue;
            
            // Calculate averages for each column (C, D, E)
            data.averages.resize(3, 0.0);
            std::vector<std::vector<double>> columnData(3);
            
            // Reorganize data by columns
            for (const auto& fileData : data.allData) {
                int rowsPerFile = fileData.size() / 3;
                for (int row = 0; row < rowsPerFile; row++) {
                    for (int col = 0; col < 3; col++) {
                        int idx = row * 3 + col;
                        if (idx < fileData.size()) {
                            columnData[col].push_back(fileData[idx]);
                        }
                    }
                }
            }
            
            // Calculate averages and quartiles
            data.q1.resize(3);
            data.q3.resize(3);
            
            for (int col = 0; col < 3; col++) {
                if (!columnData[col].empty()) {
                    data.averages[col] = std::accumulate(columnData[col].begin(), 
                                                       columnData[col].end(), 0.0) / 
                                       columnData[col].size();
                    
                    calculateQuartiles(columnData[col], data.q1[col], data.q3[col]);
                }
            }
        }
    }
    
    void printSummary() {
        std::cout << "\n=== ANALYSIS SUMMARY ===\n";
        for (const auto& pair : voltageCategories) {
            int category = pair.first;
            const VoltageData& data = pair.second;
            
            std::cout << "\nVoltage: " << (category + 8) << "kV\n";
            std::cout << "Files processed: " << data.allData.size() << "\n";
            
            if (!data.averages.empty()) {
                std::cout << "Averages - Column C: " << data.averages[0] 
                         << ", Column D: " << data.averages[1]
                         << ", Column E: " << data.averages[2] << "\n";
                
                if (data.q1.size() == 3) {
                    std::cout << "IQR (Q1-Q3) - Column C: [" << data.q1[0] << " - " << data.q3[0] << "]\n";
                    std::cout << "IQR (Q1-Q3) - Column D: [" << data.q1[1] << " - " << data.q3[1] << "]\n";
                    std::cout << "IQR (Q1-Q3) - Column E: [" << data.q1[2] << " - " << data.q3[2] << "]\n";
                }
            }
        }
    }
};

int main() {
    std::cout << "CERN CSV Data Analyzer\n";
    std::cout << "=====================\n";
    
    // Check if running as root
    if (getuid() != 0) {
        std::cout << "Warning: Not running as root. Some file access may be limited.\n";
    }
    
    CSVAnalyzer analyzer;
    analyzer.processDirectory(".");
    
    std::cout << "\nAnalysis complete. Check voltage_analysis.png for the plot.\n";
    return 0;
}