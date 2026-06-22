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
#include <unistd.h>

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
        } else if (digits.length() == 1) {
            return 1; // 3rd digit is 1 if only 1 digit
        } else {
            return -1; // Invalid (no digits found)
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
    
    // Generate ROOT script for visualization
    void generatePlot() {
        std::ofstream rootScript("plot_analysis.C");
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
        
        // Generate ROOT script
        rootScript << "#include <TCanvas.h>\n";
        rootScript << "#include <TGraph.h>\n";
        rootScript << "#include <TMultiGraph.h>\n";
        rootScript << "#include <TLegend.h>\n";
        rootScript << "#include <TAxis.h>\n";
        rootScript << "#include <TFile.h>\n";
        rootScript << "#include <iostream>\n";
        rootScript << "#include <fstream>\n";
        rootScript << "#include <vector>\n\n";
        
        rootScript << "void plot_analysis() {\n";
        rootScript << "    // Read averaged data\n";
        rootScript << "    std::ifstream avgFile(\"averaged_data.dat\");\n";
        rootScript << "    std::ifstream iqrFile(\"iqr_data.dat\");\n";
        rootScript << "    \n";
        rootScript << "    std::vector<double> voltages, colC, colD, colE;\n";
        rootScript << "    std::vector<double> q1C, q3C, q1D, q3D, q1E, q3E;\n";
        rootScript << "    \n";
        rootScript << "    std::string line;\n";
        rootScript << "    // Skip header in averaged data\n";
        rootScript << "    std::getline(avgFile, line);\n";
        rootScript << "    \n";
        rootScript << "    // Read averaged data\n";
        rootScript << "    double v, c, d, e;\n";
        rootScript << "    while (avgFile >> v >> c >> d >> e) {\n";
        rootScript << "        voltages.push_back(v);\n";
        rootScript << "        colC.push_back(c);\n";
        rootScript << "        colD.push_back(d);\n";
        rootScript << "        colE.push_back(e);\n";
        rootScript << "    }\n";
        rootScript << "    avgFile.close();\n";
        rootScript << "    \n";
        rootScript << "    // Skip header in IQR data\n";
        rootScript << "    std::getline(iqrFile, line);\n";
        rootScript << "    \n";
        rootScript << "    // Read IQR data\n";
        rootScript << "    double vIqr, q1c, q3c, q1d, q3d, q1e, q3e;\n";
        rootScript << "    while (iqrFile >> vIqr >> q1c >> q3c >> q1d >> q3d >> q1e >> q3e) {\n";
        rootScript << "        q1C.push_back(q1c);\n";
        rootScript << "        q3C.push_back(q3c);\n";
        rootScript << "        q1D.push_back(q1d);\n";
        rootScript << "        q3D.push_back(q3d);\n";
        rootScript << "        q1E.push_back(q1e);\n";
        rootScript << "        q3E.push_back(q3e);\n";
        rootScript << "    }\n";
        rootScript << "    iqrFile.close();\n";
        rootScript << "    \n";
        rootScript << "    if (voltages.empty()) {\n";
        rootScript << "        std::cout << \"No data to plot!\" << std::endl;\n";
        rootScript << "        return;\n";
        rootScript << "    }\n";
        rootScript << "    \n";
        rootScript << "    // Create canvas\n";
        rootScript << "    TCanvas *c1 = new TCanvas(\"c1\", \"CERN Voltage Analysis\", 1200, 800);\n";
        rootScript << "    c1->SetGrid();\n";
        rootScript << "    \n";
        rootScript << "    // Create graphs for averages\n";
        rootScript << "    TGraph *grC = new TGraph(voltages.size(), &voltages[0], &colC[0]);\n";
        rootScript << "    TGraph *grD = new TGraph(voltages.size(), &voltages[0], &colD[0]);\n";
        rootScript << "    TGraph *grE = new TGraph(voltages.size(), &voltages[0], &colE[0]);\n";
        rootScript << "    \n";
        rootScript << "    // Create graphs for IQR lines\n";
        rootScript << "    TGraph *grQ1C = new TGraph(voltages.size(), &voltages[0], &q1C[0]);\n";
        rootScript << "    TGraph *grQ3C = new TGraph(voltages.size(), &voltages[0], &q3C[0]);\n";
        rootScript << "    TGraph *grQ1D = new TGraph(voltages.size(), &voltages[0], &q1D[0]);\n";
        rootScript << "    TGraph *grQ3D = new TGraph(voltages.size(), &voltages[0], &q3D[0]);\n";
        rootScript << "    TGraph *grQ1E = new TGraph(voltages.size(), &voltages[0], &q1E[0]);\n";
        rootScript << "    TGraph *grQ3E = new TGraph(voltages.size(), &voltages[0], &q3E[0]);\n";
        rootScript << "    \n";
        rootScript << "    // Style the average graphs\n";
        rootScript << "    grC->SetMarkerStyle(20);\n";
        rootScript << "    grC->SetMarkerColor(kBlue);\n";
        rootScript << "    grC->SetLineColor(kBlue);\n";
        rootScript << "    grC->SetLineWidth(2);\n";
        rootScript << "    grC->SetMarkerSize(1.2);\n";
        rootScript << "    \n";
        rootScript << "    grD->SetMarkerStyle(21);\n";
        rootScript << "    grD->SetMarkerColor(kRed);\n";
        rootScript << "    grD->SetLineColor(kRed);\n";
        rootScript << "    grD->SetLineWidth(2);\n";
        rootScript << "    grD->SetMarkerSize(1.2);\n";
        rootScript << "    \n";
        rootScript << "    grE->SetMarkerStyle(22);\n";
        rootScript << "    grE->SetMarkerColor(kGreen+2);\n";
        rootScript << "    grE->SetLineColor(kGreen+2);\n";
        rootScript << "    grE->SetLineWidth(2);\n";
        rootScript << "    grE->SetMarkerSize(1.2);\n";
        rootScript << "    \n";
        rootScript << "    // Style the IQR graphs (dashed lines)\n";
        rootScript << "    grQ1C->SetLineColor(kBlue);\n";
        rootScript << "    grQ1C->SetLineStyle(2);\n";
        rootScript << "    grQ1C->SetLineWidth(1);\n";
        rootScript << "    grQ3C->SetLineColor(kBlue);\n";
        rootScript << "    grQ3C->SetLineStyle(2);\n";
        rootScript << "    grQ3C->SetLineWidth(1);\n";
        rootScript << "    \n";
        rootScript << "    grQ1D->SetLineColor(kRed);\n";
        rootScript << "    grQ1D->SetLineStyle(2);\n";
        rootScript << "    grQ1D->SetLineWidth(1);\n";
        rootScript << "    grQ3D->SetLineColor(kRed);\n";
        rootScript << "    grQ3D->SetLineStyle(2);\n";
        rootScript << "    grQ3D->SetLineWidth(1);\n";
        rootScript << "    \n";
        rootScript << "    grQ1E->SetLineColor(kGreen+2);\n";
        rootScript << "    grQ1E->SetLineStyle(2);\n";
        rootScript << "    grQ1E->SetLineWidth(1);\n";
        rootScript << "    grQ3E->SetLineColor(kGreen+2);\n";
        rootScript << "    grQ3E->SetLineStyle(2);\n";
        rootScript << "    grQ3E->SetLineWidth(1);\n";
        rootScript << "    \n";
        rootScript << "    // Create multigraph\n";
        rootScript << "    TMultiGraph *mg = new TMultiGraph();\n";
        rootScript << "    mg->Add(grC, \"LP\");\n";
        rootScript << "    mg->Add(grD, \"LP\");\n";
        rootScript << "    mg->Add(grE, \"LP\");\n";
        rootScript << "    mg->Add(grQ1C, \"L\");\n";
        rootScript << "    mg->Add(grQ3C, \"L\");\n";
        rootScript << "    mg->Add(grQ1D, \"L\");\n";
        rootScript << "    mg->Add(grQ3D, \"L\");\n";
        rootScript << "    mg->Add(grQ1E, \"L\");\n";
        rootScript << "    mg->Add(grQ3E, \"L\");\n";
        rootScript << "    \n";
        rootScript << "    mg->Draw(\"A\");\n";
        rootScript << "    mg->SetTitle(\"CERN Voltage Analysis with IQR;Voltage (kV);Average Values\");\n";
        rootScript << "    mg->GetXaxis()->SetTitle(\"Voltage (kV)\");\n";
        rootScript << "    mg->GetYaxis()->SetTitle(\"Average Values\");\n";
        rootScript << "    \n";
        rootScript << "    // Create legend\n";
        rootScript << "    TLegend *legend = new TLegend(0.1, 0.7, 0.3, 0.9);\n";
        rootScript << "    legend->AddEntry(grC, \"Column C\", \"lp\");\n";
        rootScript << "    legend->AddEntry(grD, \"Column D\", \"lp\");\n";
        rootScript << "    legend->AddEntry(grE, \"Column E\", \"lp\");\n";
        rootScript << "    legend->AddEntry(grQ1C, \"IQR Lines\", \"l\");\n";
        rootScript << "    legend->Draw();\n";
        rootScript << "    \n";
        rootScript << "    // Update canvas\n";
        rootScript << "    c1->Update();\n";
        rootScript << "    \n";
        rootScript << "    // Save as image\n";
        rootScript << "    c1->SaveAs(\"voltage_analysis_root.png\");\n";
        rootScript << "    c1->SaveAs(\"voltage_analysis_root.pdf\");\n";
        rootScript << "    \n";
        rootScript << "    std::cout << \"ROOT plot generated: voltage_analysis_root.png and .pdf\" << std::endl;\n";
        rootScript << "    std::cout << \"Canvas created. Analysis complete.\" << std::endl;\n";
        rootScript << "}\n";
        
        rootScript.close();
        
        // Execute ROOT script
        system("root -l -b -q plot_analysis.C");
        std::cout << "ROOT plot generated: voltage_analysis_root.png\n";
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
    std::cout << "CERN CSV Data Analyzer with ROOT\n";
    std::cout << "================================\n";
    
    // Check if running as root
    if (getuid() != 0) {
        std::cerr << "Error: This program must be run as root for CERN analysis.\n";
        std::cerr << "Please run with: sudo ./csv_analyzer\n";
        return 1;
    }
    
    std::cout << "Running with root privileges ✓\n";
    
    CSVAnalyzer analyzer;
    analyzer.processDirectory(".");
    
    std::cout << "\nAnalysis complete. Check voltage_analysis_root.png for the ROOT plot.\n";
    return 0;
}