#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#include "TCanvas.h"
#include "TGraph.h"
#include "TAxis.h"
#include "TLegend.h"
#include "TStyle.h"
#include "TPad.h"
#include "TLatex.h"

// Function to check if a double is valid (not inf or nan)
bool isValidDouble(double value) {
    return std::isfinite(value);
}

int main(int argc, char* argv[]) {
    std::cout << "Number of arguments: " << argc << std::endl;
    for (int i = 0; i < argc; i++) {
        std::cout << "Argument " << i << ": " << argv[i] << std::endl;
    }
    
    // Check if filename is provided
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <filename.csv>" << std::endl;
        return 1;
    }
    
    std::string filename = argv[1];
    std::cout << "Attempting to open file: " << filename << std::endl;
    
    std::ifstream file(filename);
    
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file " << filename << std::endl;
        return 1;
    }
    
    std::cout << "File opened successfully!" << std::endl;
    
    std::string line;
    int row_count = 0;
    
    // Vectors to store data for each column
    std::vector<double> col_A, col_B, col_C, col_D, col_E;
    
    // Skip first 21 rows (read up to row 21)
    while (row_count < 21 && std::getline(file, line)) {
        row_count++;
    }
    
    std::cout << "Skipped " << row_count << " rows" << std::endl;
    
    // Read data from row 22 onwards (ALL data points)
    int data_rows = 0;
    int valid_rows = 0;
    
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string cell;
        std::vector<std::string> row_data;
        
        // Parse CSV line (handle commas in data)
        while (std::getline(ss, cell, ',')) {
            row_data.push_back(cell);
        }
        
        // Check if we have enough columns
        if (row_data.size() >= 5) {
            try {
                double val_A = std::stod(row_data[0]);
                double val_B = std::stod(row_data[1]);
                double val_C = std::stod(row_data[2]);
                double val_D = std::stod(row_data[3]);
                double val_E = std::stod(row_data[4]);
                
                data_rows++;
                
                // Print progress for large files
                if (data_rows % 1000 == 0) {
                    std::cout << "Processed " << data_rows << " rows..." << std::endl;
                }
                
                // Print first few data points for debugging
                if (data_rows <= 5) {
                    std::cout << "Row " << data_rows << ": A=" << val_A 
                              << ", B=" << val_B << ", C=" << val_C 
                              << ", D=" << val_D << ", E=" << val_E << std::endl;
                }
                
                // Check if all values are valid (not inf or nan)
                if (isValidDouble(val_A) && isValidDouble(val_B) && 
                    isValidDouble(val_C) && isValidDouble(val_D) && 
                    isValidDouble(val_E)) {
                    
                    col_A.push_back(val_A);
                    col_B.push_back(val_B);
                    col_C.push_back(val_C);
                    col_D.push_back(val_D);
                    col_E.push_back(val_E);
                    valid_rows++;
                } else {
                    if (data_rows <= 10) {
                        std::cout << "Skipping invalid row " << data_rows 
                                  << " with inf/nan values" << std::endl;
                    }
                }
                
            } catch (const std::exception& e) {
                std::cerr << "Warning: Could not parse row " << (row_count + data_rows + 1) 
                          << ": " << line << std::endl;
                continue;
            }
        }
    }
    
    file.close();
    
    std::cout << "Read " << data_rows << " data rows" << std::endl;
    std::cout << "Valid data points: " << valid_rows << std::endl;
    std::cout << "Filtered out " << (data_rows - valid_rows) << " invalid rows" << std::endl;
    
    if (col_A.empty()) {
        std::cerr << "Error: No valid data found after row 22" << std::endl;
        return 1;
    }
    
    // Debug: Show data ranges
    if (!col_A.empty()) {
        double min_A = *std::min_element(col_A.begin(), col_A.end());
        double max_A = *std::max_element(col_A.begin(), col_A.end());
        std::cout << "Time (Column A) range: " << min_A << " to " << max_A << std::endl;
        
        double min_B = *std::min_element(col_B.begin(), col_B.end());
        double max_B = *std::max_element(col_B.begin(), col_B.end());
        std::cout << "Trigger (Column B) range: " << min_B << " to " << max_B << std::endl;
        
        double min_C = *std::min_element(col_C.begin(), col_C.end());
        double max_C = *std::max_element(col_C.begin(), col_C.end());
        std::cout << "Ch2 (Column C) range: " << min_C << " to " << max_C << std::endl;
        
        double min_D = *std::min_element(col_D.begin(), col_D.end());
        double max_D = *std::max_element(col_D.begin(), col_D.end());
        std::cout << "Ch3 (Column D) range: " << min_D << " to " << max_D << std::endl;
        
        double min_E = *std::min_element(col_E.begin(), col_E.end());
        double max_E = *std::max_element(col_E.begin(), col_E.end());
        std::cout << "Ch4 (Column E) range: " << min_E << " to " << max_E << std::endl;
    }
    
    std::cout << "Successfully read " << col_A.size() << " valid data points" << std::endl;
    
    // Calculate optimal canvas size (add 100 pixels width as requested)
    int optimal_width = 2000 + 100;  // Base width + 100 pixels extra
    int optimal_height = 1600;       // Height for 2x2 grid + title
    auto combined_canvas = new TCanvas("combined_canvas", "All Channels", optimal_width, optimal_height);
    
    // Create title pad (smaller portion at top)
    auto title_pad = new TPad("title_pad", "Title", 0.0, 0.92, 1.0, 1.0);
    title_pad->Draw();
    title_pad->cd();
    
    // Add title
    auto title_text = new TLatex(0.5, 0.5, "10kV, 20250717, Event #0");
    title_text->SetTextAlign(22);  // Center alignment
    title_text->SetTextSize(0.3);  // Large text
    title_text->Draw();
    
    // Create main plot area pad (use most of the space)
    auto main_pad = new TPad("main_pad", "Main Plots", 0.0, 0.0, 1.0, 0.92);
    combined_canvas->cd();
    main_pad->Draw();
    main_pad->cd();
    
    // Divide main pad into 2x2 grid with minimal spacing
    main_pad->Divide(2, 2, 0.005, 0.005);  // Very small gaps between plots
    
    // Adjust margins for each subplot to maximize plot area and prevent overlap
    for (int pad_index = 1; pad_index <= 4; pad_index++) {
        auto subplot_pad = (TPad*)main_pad->cd(pad_index);
        subplot_pad->SetLeftMargin(0.15);   // Increased for y-axis labels
        subplot_pad->SetRightMargin(0.02);
        subplot_pad->SetBottomMargin(0.15); // Increased for x-axis labels
        subplot_pad->SetTopMargin(0.05);
    }
    
    // Create separate individual canvases and add to combined
    std::vector<std::string> channel_names = {"Trigger", "Ch2", "Ch3", "Ch4"};
    std::vector<std::vector<double>*> y_data = {&col_B, &col_C, &col_D, &col_E};
    std::vector<int> colors = {kRed, kBlue, kGreen, kMagenta};
    
    std::vector<TGraph*> graphs;
    
    // First create all individual graphs
    for (int i = 0; i < 4; i++) {
        // Create graph
        auto graph = new TGraph(col_A.size());
        for (size_t j = 0; j < col_A.size(); j++) {
            graph->SetPoint(j, col_A[j], (*y_data[i])[j]);
        }
        
        // Set graph properties
        graph->SetLineColor(colors[i]);
        graph->SetLineWidth(2);
        graph->SetMarkerStyle(20);
        graph->SetMarkerColor(colors[i]);
        
        graphs.push_back(graph);
    }
    
    // Add graphs to combined canvas
    for (int i = 0; i < 4; i++) {
        main_pad->cd(i + 1);  // ROOT uses 1-based indexing
        
        // Draw graph
        graphs[i]->Draw("APL");
        
        // Set axis titles
        graphs[i]->GetXaxis()->SetTitle("Time (Column A)");
        graphs[i]->GetYaxis()->SetTitle((channel_names[i] + " Values").c_str());
        graphs[i]->SetTitle((channel_names[i] + " vs Time").c_str());
        
        // Improve readability
        graphs[i]->GetXaxis()->SetTitleSize(0.06);
        graphs[i]->GetYaxis()->SetTitleSize(0.06);
        graphs[i]->GetXaxis()->SetLabelSize(0.05);
        graphs[i]->GetYaxis()->SetLabelSize(0.05);
        graphs[i]->GetXaxis()->SetTitleOffset(0.9);
        graphs[i]->GetYaxis()->SetTitleOffset(1.1);
        graphs[i]->GetXaxis()->SetNdivisions(505);
        graphs[i]->GetYaxis()->SetNdivisions(505);
    }
    
    // Update combined canvas
    combined_canvas->Modified();
    combined_canvas->Update();
    
    // Save combined PDF (optimized size with 100 extra pixels width)
    combined_canvas->Print("all_channels_combined.pdf");
    std::cout << "Combined PDF created: all_channels_combined.pdf" << std::endl;
    std::cout << "PDF size: " << optimal_width << " x " << optimal_height << " pixels" << std::endl;
    
    // Also create individual plots with proper margins for scientific notation
    for (int i = 0; i < 4; i++) {
        // Create individual canvas
        std::string canvas_name = "canvas_" + channel_names[i];
        std::string canvas_title = channel_names[i] + " vs Time";
        auto individual_canvas = new TCanvas(canvas_name.c_str(), canvas_title.c_str(), 1000, 800);
        
        // Set optimized margins with extra space for scientific notation
        individual_canvas->SetLeftMargin(0.18);   // Increased from 0.15 to 0.18 for y-axis labels
        individual_canvas->SetRightMargin(0.05);  // Increased from 0.03 to 0.05
        individual_canvas->SetBottomMargin(0.15); // Increased from 0.15 (keep as is)
        individual_canvas->SetTopMargin(0.08);    // Increased from 0.08 (keep as is)
        
        // Draw graph
        graphs[i]->Draw("APL");
        
        // Set axis titles
        graphs[i]->GetXaxis()->SetTitle("Time (Column A)");
        graphs[i]->GetYaxis()->SetTitle((channel_names[i] + " Values").c_str());
        graphs[i]->SetTitle((channel_names[i] + " vs Time").c_str());
        
        // Improve readability with proper spacing
        graphs[i]->GetXaxis()->SetTitleSize(0.05);
        graphs[i]->GetYaxis()->SetTitleSize(0.05);
        graphs[i]->GetXaxis()->SetLabelSize(0.04);
        graphs[i]->GetYaxis()->SetLabelSize(0.04);
        graphs[i]->GetXaxis()->SetTitleOffset(1.2);
        graphs[i]->GetYaxis()->SetTitleOffset(1.6);  // Increased from 1.3 to 1.6 for better spacing
        graphs[i]->GetXaxis()->SetNdivisions(505);
        graphs[i]->GetYaxis()->SetNdivisions(505);
        
        // Update canvas
        individual_canvas->Modified();
        individual_canvas->Update();
        
        // Print canvas to file
        std::string filename_png = channel_names[i] + "_plot.png";
        std::string filename_pdf = channel_names[i] + "_plot.pdf";
        individual_canvas->Print(filename_png.c_str());
        individual_canvas->Print(filename_pdf.c_str());
        
        std::cout << channel_names[i] << " plot created: " << filename_png << " and " << filename_pdf << std::endl;
        
        // Clean up individual canvas
        delete individual_canvas;
    }
    
    std::cout << "All plots created successfully!" << std::endl;
    std::cout << "Data points plotted: " << col_A.size() << std::endl;
    std::cout << "Files generated:" << std::endl;
    std::cout << "  - all_channels_combined.pdf (combined 2x2 layout, " << optimal_width << "x" << optimal_height << " pixels)" << std::endl;
    std::cout << "  - Trigger_plot.png and Trigger_plot.pdf" << std::endl;
    std::cout << "  - Ch2_plot.png and Ch2_plot.pdf" << std::endl;
    std::cout << "  - Ch3_plot.png and Ch3_plot.pdf" << std::endl;
    std::cout << "  - Ch4_plot.png and Ch4_plot.pdf" << std::endl;
    
    // Keep the program running to see the plot
    std::cout << "Press Enter to exit..." << std::endl;
    std::cin.get();
    
    // Clean up
    delete combined_canvas;
    delete title_pad;
    delete main_pad;
    delete title_text;
    for (auto graph : graphs) {
        delete graph;
    }
    
    return 0;
}