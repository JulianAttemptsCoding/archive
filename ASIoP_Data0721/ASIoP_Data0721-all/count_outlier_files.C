// ============================================================================
// File: count_outlier_files.C  (rewritten)
// Author: ChatGPT (rewritten for robustness & RANGE_START handling)
// Description:
//   * Scans current dir for .csv files, reads columns A (time) and C+D+E (amp sum)
//   * Computes GLOBAL mean/std over ALL files
//   * For sigma = MIN_SIGMA_THRESHOLD..MAX_SIGMA_THRESHOLD, and for file-index
//     ranges [RANGE_START + n*RANGE_SIZE, RANGE_START + (n+1)*RANGE_SIZE - 1],
//     counts how many files contain any outliers (beyond n-sigma) + total count
//   * Maps each range to a kV value via index_to_kv()
//   * Builds graphs and fits (logistic vs kV, exp vs sigma per kV)
//
// How to run:
//   root
//   root [0] .L count_outlier_files.C+
//   root [1] count_outlier_files();
//
// NOTE:
//   - This version uses RANGE_START as the FIRST file index and steps by RANGE_SIZE.
//   - Includes <TAxis.h> to avoid incomplete type errors.
//   - Uses size_t for vector indices to silence signed/unsigned warnings.
// ============================================================================

#include <TSystem.h>
#include <TString.h>
#include <TCanvas.h>
#include <TGraph.h>
#include <TF1.h>
#include <TLegend.h>
#include <TMultiGraph.h>
#include <TStyle.h>
#include <TPaveText.h>
#include <TLatex.h>
#include <TAxis.h>

#include <iostream>
#include <vector>
#include <cmath>
#include <fstream>
#include <string>
#include <sstream>
#include <algorithm>
#include <limits>
#include <map>
#include <cctype>

// ------------------- CONFIGURATION -------------------
const int RANGE_START           = 30000;     // FIRST numeric file index to consider
const int RANGE_SIZE            = 100;   // Number of files per range
const int MIN_SIGMA_THRESHOLD   = 1;     // inclusive
const int MAX_SIGMA_THRESHOLD   = 5;     // inclusive
const int MIN_KV                = 8;     // mapping base
const int MAX_KV                = 14;    // mapping cap (used for axis range etc.)
const int DATA_START_ROW        = 21;    // skip first 21 lines (0-indexed)

// ------------------- DATA STRUCTS -------------------
struct AnalysisResult {
    int sigma_threshold;
    int range_start;       // file index start
    int range_end;         // file index end (inclusive)
    int kv_value;          // mapped kV
    int files_analyzed;    // # files in this range actually read
    int files_with_outliers;
    int total_outliers;    // total points beyond n-sigma across all files in range
};

// ------------------- DECLARATIONS -------------------
void count_outlier_files();

AnalysisResult process_range(int range_start,
                             int current_sigma_threshold,
                             const std::map<int, std::string>& numeric_files,
                             const std::map<int, std::vector<double>>& file_amplitudes,
                             double global_mean,
                             double global_stddev);

void create_graphs_and_fit(const std::vector<AnalysisResult>& results);

void print_final_statistics(const std::vector<AnalysisResult>& results,
                            double global_mean,
                            double global_stddev,
                            int total_csv_files,
                            int files_processed_successfully,
                            int numeric_files_count);

int  index_to_kv(int file_index);
bool read_csv_data(const char* filename,
                   std::vector<double>& time,
                   std::vector<double>& amplitude);

void calculate_stats(const std::vector<double>& data, double& mean, double& stddev);
int  count_outliers_beyond_nsigma(const std::vector<double>& data,
                                  double global_mean,
                                  double global_stddev,
                                  int sigma_threshold);

bool is_valid_number(double value);
std::string trim(const std::string& str);
bool safe_string_to_double(const std::string& str, double& result);
int  extract_numeric_index(const std::string& filename);

// ============================================================================
// MAIN
// ============================================================================
void count_outlier_files() {
    std::cout << "Starting CSV file analysis for multi-range, multi-sigma outliers..." << std::endl;
    std::cout << "Configuration: RANGE_START=" << RANGE_START
              << ", RANGE_SIZE=" << RANGE_SIZE << std::endl;
    std::cout << "Sigma thresholds: " << MIN_SIGMA_THRESHOLD << " to " << MAX_SIGMA_THRESHOLD << std::endl;
    std::cout << "kV range: " << MIN_KV << " to " << MAX_KV << std::endl;

    std::vector<double> all_amplitude_data;
    std::map<int, std::string> numeric_files;             // file_index -> filename
    std::map<int, std::vector<double>> file_amplitudes;   // file_index -> amplitudes

    int total_csv_files = 0;
    int files_processed_successfully = 0;

    TString current_dir = gSystem->pwd();
    std::cout << "Current directory: " << current_dir << std::endl;

    void* dir_handle = gSystem->OpenDirectory(current_dir);
    if (!dir_handle) {
        std::cerr << "Error: Could not open current directory." << std::endl;
        return;
    }

    const char* entry = nullptr;
    std::cout << "\n=== FIRST PASS: Reading all CSV files ===" << std::endl;
    while ((entry = gSystem->GetDirEntry(dir_handle))) {
        TString filename(entry);
        if (!filename.EndsWith(".csv")) continue;

        total_csv_files++;
        int file_index = extract_numeric_index(filename.Data());

        std::cout << "Reading CSV file: " << filename;
        if (file_index >= 0) {
            std::cout << " (index: " << file_index << ", kV: " << index_to_kv(file_index) << ")";
        } else {
            std::cout << " (non-numeric name)";
        }
        std::cout << std::endl;

        std::vector<double> file_time, file_amp;
        if (read_csv_data(filename.Data(), file_time, file_amp)) {
            files_processed_successfully++;

            if (file_index >= 0) {
                numeric_files[file_index]   = filename.Data();
                file_amplitudes[file_index] = file_amp;
            }
            all_amplitude_data.insert(all_amplitude_data.end(), file_amp.begin(), file_amp.end());
            std::cout << "  -> Successfully read " << file_amp.size() << " data points" << std::endl;
        } else {
            std::cerr << "  -> Failed to read data from " << filename << std::endl;
        }
    }
    gSystem->FreeDirectory(dir_handle);

    if (all_amplitude_data.empty()) {
        std::cerr << "Error: No valid data was read from any CSV files." << std::endl;
        return;
    }

    std::cout << "\n=== CALCULATING GLOBAL STATISTICS ===" << std::endl;
    double global_mean = 0.0, global_stddev = 0.0;
    calculate_stats(all_amplitude_data, global_mean, global_stddev);
    std::cout << "Total data points from all files: " << all_amplitude_data.size() << std::endl;
    std::cout << "Global Mean: " << global_mean << std::endl;
    std::cout << "Global Standard Deviation: " << global_stddev << std::endl;

    if (!is_valid_number(global_mean) || !is_valid_number(global_stddev) || global_stddev == 0.0) {
        std::cerr << "Error: Invalid global statistics calculated." << std::endl;
        return;
    }

    // Determine max file index present
    int max_file_index = -1;
    for (const auto& pr : numeric_files) {
        if (pr.first > max_file_index) max_file_index = pr.first;
    }
    if (max_file_index < RANGE_START) {
        std::cout << "No numeric-named files at/after RANGE_START=" << RANGE_START << std::endl;
        return;
    }

    std::vector<AnalysisResult> all_results;

    for (int current_sigma = MIN_SIGMA_THRESHOLD; current_sigma <= MAX_SIGMA_THRESHOLD; ++current_sigma) {
        std::cout << "\n" << std::string(80, '=') << std::endl;
        std::cout << "PROCESSING SIGMA THRESHOLD: " << current_sigma << std::endl;
        std::cout << std::string(80, '=') << std::endl;

        double lower = global_mean - current_sigma * global_stddev;
        double upper = global_mean + current_sigma * global_stddev;
        std::cout << "Global " << current_sigma << "-sigma bounds: [" << lower << ", " << upper << "]" << std::endl;

        for (int rs = RANGE_START; rs <= max_file_index; rs += RANGE_SIZE) {
            int kv = index_to_kv(rs);
            std::cout << "\n--- Range start " << rs << " (kV = " << kv << ") : ["
                      << rs << ", " << (rs + RANGE_SIZE - 1) << "] ---" << std::endl;

            AnalysisResult res = process_range(rs, current_sigma,
                                               numeric_files, file_amplitudes,
                                               global_mean, global_stddev);
            res.kv_value = kv;
            all_results.push_back(res);
        }
    }

    print_final_statistics(all_results, global_mean, global_stddev,
                           total_csv_files, files_processed_successfully,
                           static_cast<int>(numeric_files.size()));

    create_graphs_and_fit(all_results);
}

// ============================================================================
// HELPERS
// ============================================================================
int index_to_kv(int file_index) {
    // Map file index to kV bin assuming contiguous bins of RANGE_SIZE starting at RANGE_START
    if (file_index < RANGE_START) return MIN_KV; // fallback
    int bin = (file_index - RANGE_START) / RANGE_SIZE;
    return MIN_KV + bin;
}

AnalysisResult process_range(int range_start,
                             int current_sigma_threshold,
                             const std::map<int, std::string>& numeric_files,
                             const std::map<int, std::vector<double>>& file_amplitudes,
                             double global_mean,
                             double global_stddev) {
    AnalysisResult r{};
    r.sigma_threshold    = current_sigma_threshold;
    r.range_start        = range_start;
    r.range_end          = range_start + RANGE_SIZE - 1;
    r.files_analyzed     = 0;
    r.files_with_outliers= 0;
    r.total_outliers     = 0;

    for (int idx = range_start; idx <= r.range_end; ++idx) {
        auto itFile = numeric_files.find(idx);
        if (itFile == numeric_files.end()) continue; // no such file

        auto itAmp  = file_amplitudes.find(idx);
        if (itAmp == file_amplitudes.end()) continue; // safety

        const std::vector<double>& amps = itAmp->second;
        int outliers = count_outliers_beyond_nsigma(amps, global_mean, global_stddev, current_sigma_threshold);
        r.total_outliers += outliers;
        r.files_analyzed++;

        if (outliers > 0) {
            r.files_with_outliers++;
            std::cout << "File " << idx << ".csv (" << itFile->second << "): "
                      << outliers << " outliers" << std::endl;
        } else {
            std::cout << "File " << idx << ".csv (" << itFile->second << "): No outliers" << std::endl;
        }
    }

    std::cout << "Range [" << r.range_start << ", " << r.range_end << "] Results:" << std::endl;
    std::cout << "  Files analyzed: " << r.files_analyzed << std::endl;
    std::cout << "  Files with outliers: " << r.files_with_outliers << std::endl;
    std::cout << "  Total outliers beyond " << current_sigma_threshold << "-sigma: " << r.total_outliers << std::endl;

    return r;
}

void create_graphs_and_fit(const std::vector<AnalysisResult>& results) {
    std::cout << "\n=== CREATING GRAPHS AND PERFORMING FITS ===" << std::endl;

    gStyle->SetOptStat(0);
    gStyle->SetOptFit(1111);

    // Colors for different sigma thresholds up to 5
    int colors[6] = {kBlack, kRed, kGreen+2, kBlue, kMagenta, kOrange+7};

    // Collect unique sigma and kv values
    std::vector<int> sigmas;
    std::vector<int> kvs;
    {
        std::map<int,bool> seenSigma, seenKV;
        for (const auto& r : results) {
            if (!seenSigma[r.sigma_threshold]) { sigmas.push_back(r.sigma_threshold); seenSigma[r.sigma_threshold]=true; }
            if (!seenKV[r.kv_value]) { kvs.push_back(r.kv_value); seenKV[r.kv_value]=true; }
        }
        std::sort(sigmas.begin(), sigmas.end());
        std::sort(kvs.begin(), kvs.end());
    }

    // ----------------- Combined plot: files_with_outliers vs kV for each sigma -----------------
    TCanvas* c_combined = new TCanvas("c_combined", "Files with Outliers vs kV (All Sigma)", 1200, 800);
    c_combined->SetGrid();

    TMultiGraph* mg = new TMultiGraph();
    TLegend* leg = new TLegend(0.12, 0.65, 0.55, 0.89);
    leg->SetHeader("Sigma Thresholds", "C");

    for (int s : sigmas) {
        std::vector<double> x_kv; x_kv.reserve(kvs.size());
        std::vector<double> y_files; y_files.reserve(kvs.size());

        for (int kv : kvs) {
            // find matching result
            for (const auto& r : results) {
                if (r.sigma_threshold == s && r.kv_value == kv) {
                    x_kv.push_back(static_cast<double>(kv));
                    y_files.push_back(static_cast<double>(r.files_with_outliers));
                    break;
                }
            }
        }
        if (x_kv.size() < 3) continue; // need >=3 points for logistic fit

        TGraph* gr = new TGraph(static_cast<int>(x_kv.size()), x_kv.data(), y_files.data());
        gr->SetMarkerStyle(20 + s);
        gr->SetMarkerColor(colors[s]);
        gr->SetLineColor(colors[s]);
        gr->SetLineWidth(2);
        gr->SetMarkerSize(1.2);

        mg->Add(gr, "PL");

        TF1* fLog = new TF1(Form("logistic_sigma_%d", s), "[0]/(1+exp(-[1]*(x-[2])))+[3]", MIN_KV, MAX_KV);
        double maxY = *std::max_element(y_files.begin(), y_files.end());
        double minY = *std::min_element(y_files.begin(), y_files.end());
        double midx = 0.5*(MIN_KV + MAX_KV);
        fLog->SetParameters(maxY - minY, 1.0, midx, minY);
        fLog->SetParNames("Amplitude","Growth_Rate","Midpoint","Offset");
        fLog->SetLineColor(colors[s]);
        fLog->SetLineStyle(2);
        fLog->SetLineWidth(2);

        gr->Fit(fLog, "RQ");

        double chi2 = fLog->GetChisquare();
        int    ndf  = fLog->GetNDF();
        double r2   = (ndf>0) ? (1.0 - chi2/ndf) : 0.0; // crude R^2 proxy
        TString txt = Form("#sigma=%d: f=%.2f/(1+e^{-%.2f(x-%.2f)})+%.2f, R^{2}=%.3f",
                           s,
                           fLog->GetParameter(0),
                           fLog->GetParameter(1),
                           fLog->GetParameter(2),
                           fLog->GetParameter(3),
                           r2);
        leg->AddEntry(gr, txt, "lp");

        std::cout << "Sigma " << s << " Logistic Fit:" << std::endl;
        std::cout << "  f(x) = A/(1+exp(-B*(x-C))) + D" << std::endl;
        std::cout << "  A=" << fLog->GetParameter(0)
                  << ", B=" << fLog->GetParameter(1)
                  << ", C=" << fLog->GetParameter(2)
                  << ", D=" << fLog->GetParameter(3) << std::endl;
        std::cout << "  chi2/ndf=" << chi2 << "/" << ndf << std::endl;
        std::cout << std::endl;
    }

    c_combined->cd();
    mg->Draw("A");
    mg->SetTitle("Files with Outliers vs kV; kV; # Files with Outliers");
    mg->GetXaxis()->SetRangeUser(MIN_KV - 0.5, MAX_KV + 0.5);
    leg->Draw();

    TPaveText* title = new TPaveText(0.58, 0.75, 0.89, 0.89, "NDC");
    title->SetFillColor(0);
    title->SetBorderSize(1);
    title->AddText("Logistic Fit: f(x)=A/(1+e^{-B(x-C)})+D");
    title->Draw();

    c_combined->Update();
    c_combined->SaveAs("outliers_vs_kv_all_sigma.png");
    c_combined->SaveAs("outliers_vs_kv_all_sigma.pdf");

    // ----------------- Per-kV plots: files_with_outliers vs sigma -----------------
    for (int kv : kvs) {
        std::vector<double> x_sigma;
        std::vector<double> y_files;
        for (int s : sigmas) {
            for (const auto& r : results) {
                if (r.kv_value == kv && r.sigma_threshold == s) {
                    x_sigma.push_back(static_cast<double>(s));
                    y_files.push_back(static_cast<double>(r.files_with_outliers));
                    break;
                }
            }
        }
        if (x_sigma.size() < 3) continue;

        TCanvas* c_kv = new TCanvas(Form("c_kv_%d", kv), Form("%d kV: Files with Outliers vs Sigma", kv), 800, 600);
        c_kv->SetGrid();

        TGraph* grKV = new TGraph(static_cast<int>(x_sigma.size()), x_sigma.data(), y_files.data());
        grKV->SetMarkerStyle(21);
        grKV->SetMarkerColor(kRed);
        grKV->SetLineColor(kRed);
        grKV->SetLineWidth(2);
        grKV->SetMarkerSize(1.4);
        grKV->SetTitle(Form("%d kV: Files with Outliers vs Sigma; Sigma Threshold; # Files with Outliers", kv));

        TF1* fExp = new TF1(Form("exp_fit_kv_%d", kv), "[0]*exp(-[1]*x)+[2]", MIN_SIGMA_THRESHOLD, MAX_SIGMA_THRESHOLD);
        double maxY = *std::max_element(y_files.begin(), y_files.end());
        double minY = *std::min_element(y_files.begin(), y_files.end());
        fExp->SetParameters(maxY - minY, 0.5, minY);
        fExp->SetLineColor(kBlue);
        fExp->SetLineWidth(2);

        grKV->Draw("APL");
        grKV->Fit(fExp, "RQ");

        double chi2 = fExp->GetChisquare();
        int    ndf  = fExp->GetNDF();
        double r2   = (ndf>0) ? (1.0 - chi2/ndf) : 0.0;

        TLatex latex;
        latex.SetNDC();
        latex.SetTextSize(0.04);
        latex.DrawLatex(0.15, 0.85, Form("f(x)=%.2f e^{-%.2f x} + %.2f",
                                         fExp->GetParameter(0),
                                         fExp->GetParameter(1),
                                         fExp->GetParameter(2)));
        latex.DrawLatex(0.15, 0.80, Form("R^{2}=%.3f", r2));

        c_kv->Update();
        c_kv->SaveAs(Form("outliers_vs_sigma_%dkv.png", kv));
        c_kv->SaveAs(Form("outliers_vs_sigma_%dkv.pdf", kv));
    }

    std::cout << "Graphs saved as PNG and PDF files." << std::endl;
}

void print_final_statistics(const std::vector<AnalysisResult>& results,
                            double global_mean,
                            double global_stddev,
                            int total_csv_files,
                            int files_processed_successfully,
                            int numeric_files_count) {
    std::cout << "\n" << std::string(100, '=') << std::endl;
    std::cout << "COMPREHENSIVE FINAL STATISTICS" << std::endl;
    std::cout << std::string(100, '=') << std::endl;

    std::cout << "\n--- FILE PROCESSING SUMMARY ---" << std::endl;
    std::cout << "Total CSV files found: " << total_csv_files << std::endl;
    std::cout << "Files successfully processed: " << files_processed_successfully << std::endl;
    std::cout << "Files with numeric names found: " << numeric_files_count << std::endl;

    std::cout << "\n--- GLOBAL STATISTICS (from ALL files) ---" << std::endl;
    std::cout << "Global Mean: " << global_mean << std::endl;
    std::cout << "Global Standard Deviation: " << global_stddev << std::endl;

    std::cout << "\n--- DETAILED OUTLIER ANALYSIS RESULTS BY kV ---" << std::endl;
    std::cout << "Sigma |   kV  | Files Analyzed | Files w/ Outliers | Total Outliers" << std::endl;
    std::cout << std::string(75, '-') << std::endl;

    for (const auto& r : results) {
        printf("%5d | %5d | %14d | %17d | %14d\n",
               r.sigma_threshold, r.kv_value,
               r.files_analyzed, r.files_with_outliers, r.total_outliers);
    }

    std::cout << "\n--- SUMMARY BY SIGMA THRESHOLD ---" << std::endl;
    for (int s = MIN_SIGMA_THRESHOLD; s <= MAX_SIGMA_THRESHOLD; ++s) {
        int tot_files = 0, tot_files_w = 0, tot_outliers = 0, kv_count = 0;
        for (const auto& r : results) {
            if (r.sigma_threshold == s) {
                tot_files     += r.files_analyzed;
                tot_files_w   += r.files_with_outliers;
                tot_outliers  += r.total_outliers;
                kv_count++;
            }
        }
        std::cout << s << "-Sigma Threshold:" << std::endl;
        std::cout << "  kV ranges processed: " << kv_count << std::endl;
        std::cout << "  Total files analyzed: " << tot_files << std::endl;
        std::cout << "  Total files with outliers: " << tot_files_w << std::endl;
        std::cout << "  Total outliers found: " << tot_outliers << std::endl;
        if (tot_files > 0) {
            std::cout << "  % files with outliers: "
                      << (100.0 * tot_files_w / tot_files) << "%" << std::endl;
        }
        std::cout << std::endl;
    }

    std::cout << "--- SUMMARY BY kV (across all sigmas) ---" << std::endl;
    std::map<int, std::vector<AnalysisResult>> byKV;
    for (const auto& r : results) byKV[r.kv_value].push_back(r);

    for (const auto& pair : byKV) {
        int kv = pair.first;
        const auto& vec = pair.second;
        std::cout << "kV " << kv << ":" << std::endl;
        int files_in_range = vec.empty()?0:vec.front().files_analyzed;
        std::cout << "  Files in range: " << files_in_range << std::endl;
        for (const auto& r : vec) {
            std::cout << "  " << r.sigma_threshold << "-sigma: "
                      << r.total_outliers << " outliers in "
                      << r.files_with_outliers << "/" << r.files_analyzed << " files" << std::endl;
        }
        std::cout << std::endl;
    }

    int grand_total_outliers = 0;
    for (const auto& r : results) grand_total_outliers += r.total_outliers;

    std::cout << "--- GRAND TOTALS ---" << std::endl;
    std::cout << "Total analyses performed: " << results.size() << std::endl;
    std::cout << "Grand total outliers found: " << grand_total_outliers << std::endl;
    std::cout << "\n" << std::string(100, '=') << std::endl;
}

// ----------------------------------- CSV read -----------------------------------
bool read_csv_data(const char* filename, std::vector<double>& time, std::vector<double>& amplitude) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error opening CSV file: " << filename << std::endl;
        return false;
    }

    std::string line;
    int line_num = 0;
    int valid_rows = 0;

    while (std::getline(file, line)) {
        line_num++;
        if (line_num <= DATA_START_ROW) continue; // skip header/metadata

        if (trim(line).empty()) continue;

        std::stringstream ss(line);
        std::string seg;
        std::vector<double> values;
        while (std::getline(ss, seg, ',')) {
            double v;
            if (safe_string_to_double(trim(seg), v)) values.push_back(v);
            else values.push_back(0.0);
        }
        if (values.size() >= 5) {
            double t  = values[0];   // A
            double amp= values[2] + values[3] + values[4]; // C+D+E
            if (is_valid_number(t) && is_valid_number(amp)) {
                time.push_back(t);
                amplitude.push_back(amp);
                valid_rows++;
            }
        }
    }
    file.close();
    return (valid_rows > 0);
}

// ----------------------------------- Stats helpers -----------------------------------
void calculate_stats(const std::vector<double>& data, double& mean, double& stddev) {
    if (data.empty()) {
        mean = stddev = 0.0;
        return;
    }
    double sum = 0.0;
    for (double x : data) sum += x;
    mean = sum / data.size();

    double sq = 0.0;
    for (double x : data) {
        double d = x - mean;
        sq += d*d;
    }
    stddev = std::sqrt(sq / data.size());
}

int count_outliers_beyond_nsigma(const std::vector<double>& data,
                                 double global_mean,
                                 double global_stddev,
                                 int sigma_threshold) {
    if (data.empty()) return 0;
    if (!is_valid_number(global_mean) || !is_valid_number(global_stddev) || global_stddev == 0.0)
        return 0;

    double low = global_mean - sigma_threshold * global_stddev;
    double high= global_mean + sigma_threshold * global_stddev;
    int count = 0;
    for (double v : data) {
        if (v < low || v > high) count++;
    }
    return count;
}

bool is_valid_number(double value) {
    return std::isfinite(value) && !std::isnan(value);
}

std::string trim(const std::string& s) {
    size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e-b+1);
}

bool safe_string_to_double(const std::string& str, double& result) {
    if (str.empty()) return false;
    try {
        size_t pos = 0;
        result = std::stod(str, &pos);
        if (pos != str.length()) return false;
        return is_valid_number(result);
    } catch (...) {
        return false;
    }
}

int extract_numeric_index(const std::string& filename) {
    std::string base = filename;
    size_t dot = base.find_last_of('.');
    if (dot != std::string::npos) base = base.substr(0, dot);
    if (base.empty()) return -1;
    for (char c : base) if (!std::isdigit(static_cast<unsigned char>(c))) return -1;
    try {
        return std::stoi(base);
    } catch (...) {
        return -1;
    }
}

// ============================================================================
// End of file
// ============================================================================
