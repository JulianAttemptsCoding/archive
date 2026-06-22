#include <TSystemDirectory.h>
#include <TSystemFile.h>
#include <TList.h>
#include <TString.h>

#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <cmath>
#include <numeric>
#include <algorithm>

void csv_threshold_filter() {
    const int n = 3;  // ⬅️ change this to desired n-sigma threshold

    std::vector<double> all_averages;

    TSystemDirectory dir(".", ".");
    TList *files = dir.GetListOfFiles();
    if (!files) return;

    TIter next(files);
    TSystemFile *file;

    while ((file = (TSystemFile*)next())) {
        TString fname = file->GetName();
        if (!file->IsDirectory() && fname.EndsWith(".csv")) {
            std::ifstream csv(fname.Data());
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
                    if (col >= 2 && col <= 4) {
                        try {
                            values[count++] = std::stod(token);
                        } catch (...) {}
                    }
                    col++;
                }

                if (count == 3) {
                    double avg = (values[0] + values[1] + values[2]) / 3.0;
                    all_averages.push_back(avg);
                }
            }
        }
    }

    if (all_averages.empty()) {
        std::cout << "No data found.\n";
        return;
    }

    // Compute mean
    double mean = std::accumulate(all_averages.begin(), all_averages.end(), 0.0) / all_averages.size();

    // Compute standard deviation
    double variance = 0.0;
    for (double v : all_averages) {
        variance += (v - mean) * (v - mean);
    }
    variance /= all_averages.size();
    double sigma = std::sqrt(variance);

    // Compute n-sigma threshold bounds
    double lower = mean - n * sigma;
    double upper = mean + n * sigma;

    std::cout << "=== GLOBAL STATISTICS ===\n";
    std::cout << "Total Values: " << all_averages.size() << "\n";
    std::cout << "Mean         : " << mean << "\n";
    std::cout << "Std Dev (σ)  : " << sigma << "\n";
    std::cout << n << "-Sigma Range : [" << lower << ", " << upper << "]\n";
}
