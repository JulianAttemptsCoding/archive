#include <TCanvas.h>
#include <TGraphErrors.h>
#include <TAxis.h>
#include <TStyle.h>
#include <TSystemDirectory.h>
#include <TSystemFile.h>
#include <TList.h>
#include <TString.h>

#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <regex>

void csv_analyzer() {
    const int N = 7; // 8kV to 14kV → 7 categories
    std::vector<std::vector<double>> data_per_kv(N);

    // Scan current directory for .csv files
    TSystemDirectory dir(".", ".");
    TList *files = dir.GetListOfFiles();
    if (!files) return;

    TIter next(files);
    TSystemFile *file;

    while ((file = (TSystemFile*)next())) {
        TString fname = file->GetName();
        if (!file->IsDirectory() && fname.EndsWith(".csv")) {
            std::string name = fname.Data();

            // Extract digits from filename
            std::string digits;
            for (char ch : name) {
                if (isdigit(ch)) digits += ch;
            }

            int digit = 0;
            if (digits.size() >= 3)
                digit = digits[digits.size() - 3] - '0';

            // Map 0→8kV, 1→9kV, ..., 6→14kV
            if (digit >= 0 && digit <= 6) {
                int index = digit; // maps directly to 0 → 8kV, ..., 6 → 14kV
                std::ifstream csv(name);
                std::string line;
                int row = 0;
                while (std::getline(csv, line)) {
                    row++;
                    if (row < 22 || row > 98) continue;

                    std::stringstream ss(line);
                    std::string token;
                    int col = 0;
                    double values[3];
                    int count = 0;

                    while (std::getline(ss, token, ',')) {
                        if (col >= 2 && col <= 4) { // columns C to E
                            try {
                                values[count++] = std::stod(token);
                            } catch (...) {}
                        }
                        col++;
                    }

                    if (count == 3) {
                        double avg = (values[0] + values[1] + values[2]) / 3.0;
                        data_per_kv[index].push_back(avg);
                    }
                }
            }
        }
    }

    // Prepare data for graph
    std::vector<double> x, y, y_err;

    for (int i = 0; i < N; ++i) {
        if (data_per_kv[i].empty()) continue;

        std::vector<double>& vec = data_per_kv[i];
        double mean = std::accumulate(vec.begin(), vec.end(), 0.0) / vec.size();

        // Compute standard deviation (1 sigma)
        double variance = 0.0;
        for (double v : vec) {
            variance += (v - mean) * (v - mean);
        }
        variance /= vec.size();  // population standard deviation
        double sigma = std::sqrt(variance);

        x.push_back(8 + i);      // kV value
        y.push_back(mean);       // mean value
        y_err.push_back(sigma);  // ±1σ error bar
    }

    // Plot results
    TCanvas *c = new TCanvas("c", "Average per kV Category", 800, 600);
    TGraphErrors *gr = new TGraphErrors(x.size());

    for (size_t i = 0; i < x.size(); ++i) {
        gr->SetPoint(i, x[i], y[i]);
        gr->SetPointError(i, 0, y_err[i]);
    }

    gr->SetTitle("Average Signal by High Voltage;High Voltage [kV];Average Value");
    gr->SetMarkerStyle(21);
    gr->SetMarkerSize(1.2);
    gr->SetLineWidth(2);
    gr->SetLineColor(kBlue + 1);
    gr->SetMarkerColor(kBlue + 1);

    gr->Draw("APL");

    c->Update();
}
