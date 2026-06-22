#include <TCanvas.h>
#include <TGraph.h>
#include <TMultiGraph.h>
#include <TLegend.h>
#include <TAxis.h>
#include <TFile.h>
#include <iostream>
#include <fstream>
#include <vector>

void plot_analysis() {
    // Read averaged data
    std::ifstream avgFile("averaged_data.dat");
    std::ifstream iqrFile("iqr_data.dat");
    
    std::vector<double> voltages, colC, colD, colE;
    std::vector<double> q1C, q3C, q1D, q3D, q1E, q3E;
    
    std::string line;
    // Skip header in averaged data
    std::getline(avgFile, line);
    
    // Read averaged data
    double v, c, d, e;
    while (avgFile >> v >> c >> d >> e) {
        voltages.push_back(v);
        colC.push_back(c);
        colD.push_back(d);
        colE.push_back(e);
    }
    avgFile.close();
    
    // Skip header in IQR data
    std::getline(iqrFile, line);
    
    // Read IQR data
    double vIqr, q1c, q3c, q1d, q3d, q1e, q3e;
    while (iqrFile >> vIqr >> q1c >> q3c >> q1d >> q3d >> q1e >> q3e) {
        q1C.push_back(q1c);
        q3C.push_back(q3c);
        q1D.push_back(q1d);
        q3D.push_back(q3d);
        q1E.push_back(q1e);
        q3E.push_back(q3e);
    }
    iqrFile.close();
    
    if (voltages.empty()) {
        std::cout << "No data to plot!" << std::endl;
        return;
    }
    
    // Create canvas
    TCanvas *c1 = new TCanvas("c1", "CERN Voltage Analysis", 1200, 800);
    c1->SetGrid();
    
    // Create graphs for averages
    TGraph *grC = new TGraph(voltages.size(), &voltages[0], &colC[0]);
    TGraph *grD = new TGraph(voltages.size(), &voltages[0], &colD[0]);
    TGraph *grE = new TGraph(voltages.size(), &voltages[0], &colE[0]);
    
    // Create graphs for IQR lines
    TGraph *grQ1C = new TGraph(voltages.size(), &voltages[0], &q1C[0]);
    TGraph *grQ3C = new TGraph(voltages.size(), &voltages[0], &q3C[0]);
    TGraph *grQ1D = new TGraph(voltages.size(), &voltages[0], &q1D[0]);
    TGraph *grQ3D = new TGraph(voltages.size(), &voltages[0], &q3D[0]);
    TGraph *grQ1E = new TGraph(voltages.size(), &voltages[0], &q1E[0]);
    TGraph *grQ3E = new TGraph(voltages.size(), &voltages[0], &q3E[0]);
    
    // Style the average graphs
    grC->SetMarkerStyle(20);
    grC->SetMarkerColor(kBlue);
    grC->SetLineColor(kBlue);
    grC->SetLineWidth(2);
    grC->SetMarkerSize(1.2);
    
    grD->SetMarkerStyle(21);
    grD->SetMarkerColor(kRed);
    grD->SetLineColor(kRed);
    grD->SetLineWidth(2);
    grD->SetMarkerSize(1.2);
    
    grE->SetMarkerStyle(22);
    grE->SetMarkerColor(kGreen+2);
    grE->SetLineColor(kGreen+2);
    grE->SetLineWidth(2);
    grE->SetMarkerSize(1.2);
    
    // Style the IQR graphs (dashed lines)
    grQ1C->SetLineColor(kBlue);
    grQ1C->SetLineStyle(2);
    grQ1C->SetLineWidth(1);
    grQ3C->SetLineColor(kBlue);
    grQ3C->SetLineStyle(2);
    grQ3C->SetLineWidth(1);
    
    grQ1D->SetLineColor(kRed);
    grQ1D->SetLineStyle(2);
    grQ1D->SetLineWidth(1);
    grQ3D->SetLineColor(kRed);
    grQ3D->SetLineStyle(2);
    grQ3D->SetLineWidth(1);
    
    grQ1E->SetLineColor(kGreen+2);
    grQ1E->SetLineStyle(2);
    grQ1E->SetLineWidth(1);
    grQ3E->SetLineColor(kGreen+2);
    grQ3E->SetLineStyle(2);
    grQ3E->SetLineWidth(1);
    
    // Create multigraph
    TMultiGraph *mg = new TMultiGraph();
    mg->Add(grC, "LP");
    mg->Add(grD, "LP");
    mg->Add(grE, "LP");
    mg->Add(grQ1C, "L");
    mg->Add(grQ3C, "L");
    mg->Add(grQ1D, "L");
    mg->Add(grQ3D, "L");
    mg->Add(grQ1E, "L");
    mg->Add(grQ3E, "L");
    
    mg->Draw("A");
    mg->SetTitle("CERN Voltage Analysis with IQR;Voltage (kV);Average Values");
    mg->GetXaxis()->SetTitle("Voltage (kV)");
    mg->GetYaxis()->SetTitle("Average Values");
    
    // Create legend
    TLegend *legend = new TLegend(0.1, 0.7, 0.3, 0.9);
    legend->AddEntry(grC, "Column C", "lp");
    legend->AddEntry(grD, "Column D", "lp");
    legend->AddEntry(grE, "Column E", "lp");
    legend->AddEntry(grQ1C, "IQR Lines", "l");
    legend->Draw();
    
    // Update canvas
    c1->Update();
    
    // Save as image
    c1->SaveAs("voltage_analysis_root.png");
    c1->SaveAs("voltage_analysis_root.pdf");
    
    std::cout << "ROOT plot generated: voltage_analysis_root.png and .pdf" << std::endl;
    std::cout << "Canvas created. Analysis complete." << std::endl;
}
