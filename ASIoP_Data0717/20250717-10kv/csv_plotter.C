#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include "TCanvas.h"
#include "TGraph.h"
#include "TMultiGraph.h"
#include "TAxis.h"
#include "TLegend.h"
#include "TApplication.h"

int main(int argc, char* argv[]) {
    // Initialize ROOT application
    TApplication app("app", &argc, argv);
    
    // Check if filename is provided
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <filename.csv>" << std::endl;
        return 1;
    }
    
    std::string filename = argv[1];
    std::ifstream file(filename);
    
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file " << filename << std::endl;
        return 1;
    }
    
    std::string line;
    int row_count = 0;
    
    // Vectors to store data for each column
    std::vector<double> col_A, col_B, col_C, col_D, col_E;
    
    // Skip first 21 rows (read up to row 21)
    while (row_count < 21 && std::getline(file, line)) {
        row_count++;
    }
    
    // Read data from row 22 onwards
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
                
                col_A.push_back(val_A);
                col_B.push_back(val_B);
                col_C.push_back(val_C);
                col_D.push_back(val_D);
                col_E.push_back(val_E);
            } catch (const std::exception& e) {
                std::cerr << "Warning: Could not parse row data: " << line << std::endl;
                continue;
            }
        }
    }
    
    file.close();
    
    if (col_A.empty()) {
        std::cerr << "Error: No valid data found after row 22" << std::endl;
        return 1;
    }
    
    // Create graphs for each Y column (B, C, D, E) vs X column (A)
    TGraph* graph_B = new TGraph(col_A.size());
    TGraph* graph_C = new TGraph(col_A.size());
    TGraph* graph_D = new TGraph(col_A.size());
    TGraph* graph_E = new TGraph(col_A.size());
    
    for (size_t i = 0; i < col_A.size(); i++) {
        graph_B->SetPoint(i, col_A[i], col_B[i]);
        graph_C->SetPoint(i, col_A[i], col_C[i]);
        graph_D->SetPoint(i, col_A[i], col_D[i]);
        graph_E->SetPoint(i, col_A[i], col_E[i]);
    }
    
    // Set graph properties
    graph_B->SetLineColor(kRed);
    graph_B->SetLineWidth(2);
    graph_B->SetMarkerStyle(20);
    graph_B->SetMarkerColor(kRed);
    
    graph_C->SetLineColor(kBlue);
    graph_C->SetLineWidth(2);
    graph_C->SetMarkerStyle(21);
    graph_C->SetMarkerColor(kBlue);
    
    graph_D->SetLineColor(kGreen);
    graph_D->SetLineWidth(2);
    graph_D->SetMarkerStyle(22);
    graph_D->SetMarkerColor(kGreen);
    
    graph_E->SetLineColor(kMagenta);
    graph_E->SetLineWidth(2);
    graph_E->SetMarkerStyle(23);
    graph_E->SetMarkerColor(kMagenta);
    
    // Create canvas and multigraph
    TCanvas* canvas = new TCanvas("canvas", "CSV Data Plot", 1200, 800);
    TMultiGraph* mg = new TMultiGraph();
    
    // Add graphs to multigraph
    mg->Add(graph_B);
    mg->Add(graph_C);
    mg->Add(graph_D);
    mg->Add(graph_E);
    
    // Draw the multigraph
    mg->Draw("APL");
    
    // Set axis titles
    mg->GetXaxis()->SetTitle("Column A");
    mg->GetYaxis()->SetTitle("Values (Columns B-E)");
    mg->SetTitle("Data Plot: Columns B-E vs Column A");
    
    // Create legend
    TLegend* legend = new TLegend(0.7, 0.7, 0.9, 0.9);
    legend->AddEntry(graph_B, "Column B", "lp");
    legend->AddEntry(graph_C, "Column C", "lp");
    legend->AddEntry(graph_D, "Column D", "lp");
    legend->AddEntry(graph_E, "Column E", "lp");
    legend->Draw();
    
    // Update canvas
    canvas->Update();
    
    // Print canvas to file
    canvas->Print("csv_plot.png");
    canvas->Print("csv_plot.pdf");
    
    std::cout << "Plot created successfully!" << std::endl;
    std::cout << "Data points plotted: " << col_A.size() << std::endl;
    
    // Run the application (keeps the window open)
    app.Run();
    
    return 0;
}