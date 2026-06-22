//filename: improved_ana.cc

#include <cstdint>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <cstring>
#include <algorithm>
#include <vector>
#include <filesystem> // C++17 for better path handling
#include <sstream>
#include <string>
#include <memory> // For smart pointers
#include <map>    // To store efficiency results
#include <set>    // For sorted filenames
#include <regex>  // For extracting numbers from filename
#include <tuple>  // For sorting with extracted numbers
#include "TROOT.h"
#include "TFile.h"
#include "TTree.h"
#include "TH1.h"
#include "TH1D.h"
#include "TH2D.h" // For efficiency plots
#include "TProfile.h" // For efficiency plots
#include "TCanvas.h"
#include "TStyle.h"
#include "TLegend.h"
#include "TPaveText.h"
#include "TLine.h" // Include the header for TLine
#include "TGraphErrors.h" // Include for TGraphErrors
#include "TLatex.h" // Include for TLatex (axis labels)
#include "TF1.h" // Include for TF1 (fit functions)

// ===== Parameters =====
static constexpr double MEAS_LSB_NS = 25.0 / 1024.0; // ~0.0244 ns per LSB
static constexpr double TRIG_LSB_NS = 25.0;          // 25 ns per trigger LSB

// ===== Identify tags =====
enum : uint32_t {
    TAG_MEAS = 0x0,
    TAG_TDC_HDR = 0x1,
    TAG_TDC_TRL = 0x3,
    TAG_GBL_HDR = 0x8,
    TAG_GBL_TRL = 0x10,
    TAG_TTT = 0x11
};

// ===== Structure =====
struct event_v1290 {
    uint32_t board_identify = 0;
    uint32_t event_id = 0;
    uint16_t error_flag = 0;
    uint32_t trigger_time = 0; // ETTT(MSB)<<5 + GT(LSB)
    int hit_count[32] = {0};
    uint8_t tdc_trailing[32][1024] = {{0}}; // 0=leading, 1=trailing
    uint32_t tdc_measurement[32][1024] = {{0}};
    uint16_t word_count = 0;
    uint16_t tdc_event_id_12b = 0;
    uint16_t bunch_id = 0;
};

// ===== Decode a 32-bit word =====
static int Decode(uint32_t data, event_v1290* ev) {
    uint8_t identify = data >> 27;
    switch (identify) {
        case TAG_MEAS: { // 0x0
            uint8_t tr = (data & 0x04000000) ? 1 : 0; // bit 26
            int ch = (data >> 21) & 0x1F; // 25..21
            if (ch < 0 || ch >= 32) break;
            uint32_t val = data & 0x1FFFFF; // 20..0
            int &hc = ev->hit_count[ch];
            if (hc < 1024) {
                ev->tdc_trailing[ch][hc] = tr;
                ev->tdc_measurement[ch][hc] = val;
                ++hc;
            }
            break;
        }
        case TAG_TDC_HDR: { // 0x1
            ev->tdc_event_id_12b = (data >> 12) & 0x0FFF;
            ev->bunch_id = data & 0x0FFF;
            break;
        }
        case TAG_TDC_TRL: { // 0x3
            ev->word_count = data & 0x0FFF;
            break;
        }
        case 0x4: { // TDC Error
            ev->error_flag = data & 0x7FFF;
            break;
        }
        case TAG_GBL_HDR: { // 0x8
            ev->event_id = (data >> 5) & 0x3FFFFF;
            break;
        }
        case TAG_GBL_TRL: { // 0x10
            ev->word_count = (data >> 5) & 0xFFFF;
            ev->trigger_time |= (data & 0x1F); // LSB
            return 1; // Event complete
        }
        case TAG_TTT: { // 0x11
            ev->trigger_time = (data & 0x7FFFFFF) << 5; // MSB
            break;
        }
        case 0x18: // Filler
        case 0x1F: // User defined
            // ev->board_identify = data & 0xFFFF0000;
            break;
        default:
            // std::cerr << "Warning: Unknown tag 0x" << std::hex << (int)identify << std::dec << std::endl; // Optional: Log unknown tags
            break;
    }
    return 0;
}

// Function to determine output filenames based on input
std::pair<std::string, std::string> generate_output_names(const std::string& input_filename) {
    std::filesystem::path input_path(input_filename);
    std::string base_name = input_path.stem().string(); // Get filename without extension
    std::string root_filename = base_name + ".root";
    std::string pdf_filename = base_name + ".pdf";
    return {root_filename, pdf_filename};
}

// Structure to hold efficiency results
struct EfficiencyResult {
    double efficiency = 0.0;
    double error = 0.0;
    int numerator = 0;
    int denominator = 0;
};

// Function to calculate efficiency based on the new definition
// Efficiency = (Number of events with at least one hit) / (Total number of valid events)
EfficiencyResult calculate_efficiency(TTree* tstat) {
    EfficiencyResult result;
    if (!tstat) {
        std::cerr << "Error: event_stats tree is null, cannot calculate efficiency." << std::endl;
        return result;
    }

    // Variables to read from the tree
    UInt_t s_event_id = 0;
    Bool_t s_trigger_present = kTRUE;
    Int_t s_n_leading[32] = {0}; // We will use leading edges for hit detection
    tstat->SetBranchAddress("event_id", &s_event_id);
    tstat->SetBranchAddress("trigger_present", &s_trigger_present);
    tstat->SetBranchAddress("n_leading", s_n_leading);
    Long64_t n_entries = tstat->GetEntries();
    if (n_entries == 0) {
        std::cerr << "Warning: event_stats tree is empty, no efficiency to calculate." << std::endl;
        return result;
    }

    // --- Efficiency Calculation Logic ---
    // Definition: Efficiency = (Events with at least one hit on any channel) / (Total valid events)
    // Note: Channels with zero hits are ignored in the *calculation* of the numerator,
    // but they still contribute to the denominator (total events).
    int total_valid_events = 0; // Events with a valid trigger time (s_trigger_present == true)
    int total_events_with_any_hit = 0; // Events where at least one channel had a hit (s_n_leading[ch] > 0 for any ch)
    for (Long64_t i = 0; i < n_entries; i++) {
        tstat->GetEntry(i);
        // Only consider events where a trigger time was present
        if (s_trigger_present) {
            total_valid_events++;
            bool event_has_any_hit = false; // Flag for this specific event
            for (int ch = 0; ch < 32; ch++) {
                // Check if this channel fired in this event (using leading edge count)
                if (s_n_leading[ch] > 0) {
                    event_has_any_hit = true;
                    // Break early as we only need to know if *any* channel fired
                    break;
                }
            }
            // If at least one channel fired in this event, increment the counter
            if (event_has_any_hit) {
                total_events_with_any_hit++;
            }
        }
    }

    // --- Calculate Final Efficiency ---
    result.denominator = total_valid_events;
    result.numerator = total_events_with_any_hit;

    if (total_valid_events > 0) {
        result.efficiency = static_cast<double>(total_events_with_any_hit) / total_valid_events;
        // Simple binomial error for efficiency
        result.error = (result.efficiency * (1.0 - result.efficiency) / total_valid_events > 0) ?
                          sqrt(result.efficiency * (1.0 - result.efficiency) / total_valid_events) : 0.0;
    }
    return result;
}

// Helper function to extract the first number from a filename
double extract_voltage_from_filename(const std::string& filename) {
    std::regex number_regex(R"(\d+)");
    std::smatch match;
    std::string fname = std::filesystem::path(filename).stem().string(); // Get filename without extension

    if (std::regex_search(fname, match, number_regex)) {
        try {
            return std::stod(match.str(0));
        } catch (const std::exception& e) {
            std::cerr << "Warning: Could not convert found number '" << match.str(0) << "' to double in filename '" << filename << "'. Using 0.0.\n";
            return 0.0;
        }
    } else {
        std::cerr << "Warning: No number found in filename '" << filename << "'. Using 0.0.\n";
        return 0.0;
    }
}

// Function to perform analysis on a single file
// Returns the efficiency result for that file
EfficiencyResult analyze_single_file(const std::string& input_filename, bool need_bswap) {
    std::cout << "\n--- Analyzing file: " << input_filename << " ---" << std::endl;

    auto names = generate_output_names(input_filename);
    std::string output_root_name = names.first; // Use .root name directly

    // ==== Open Input File ====
    std::ifstream ifile(input_filename, std::ios::binary);
    if (!ifile) {
        std::cerr << "Error: Opening input file " << input_filename << " failed\n";
        return {}; // Return default (invalid) result
    }
    ifile.seekg(0, std::ios::end);
    auto fsize = ifile.tellg();
    ifile.seekg(0, std::ios::beg);
    if (fsize <= 0) {
        std::cerr << "Error: Input file " << input_filename << " is empty\n";
        return {};
    }
    std::cout << "Input file: " << input_filename << std::endl;
    std::cout << "File size = " << fsize << " bytes (" << (fsize/4) << " words)\n";

    // ==== Open ROOT Output File ====
    std::unique_ptr<TFile> fout(TFile::Open(output_root_name.c_str(), "RECREATE"));
    if (!fout || fout->IsZombie()) {
        std::cerr << "Error: Could not create ROOT output file " << output_root_name << std::endl;
        return {};
    }
    std::cout << "Output ROOT file: " << output_root_name << std::endl;

    // ==== Setup ROOT Trees and Histograms ====
    // --- hits Tree ---
    UInt_t h_identify=0, h_trailing=0, h_ch=0, h_tdc_raw=0, h_event_id=0;
    Double_t h_t_ns=0.0, h_trigger_ns=0.0, h_dt_ns=0.0;
    TTree* thits = new TTree("hits","V1290 hits");
    thits->Branch("identify", &h_identify, "identify/i");
    thits->Branch("trailing", &h_trailing, "trailing/i");
    thits->Branch("channel_id", &h_ch, "channel_id/i");
    thits->Branch("tdc_raw", &h_tdc_raw, "tdc_raw/i");
    thits->Branch("t_ns", &h_t_ns, "t_ns/D");
    thits->Branch("trigger_ns", &h_trigger_ns, "trigger_ns/D");
    thits->Branch("dt_ns", &h_dt_ns, "dt_ns/D");
    thits->Branch("event_id", &h_event_id, "event_id/i");

    // --- events Tree ---
    UInt_t e_event_id=0, e_word_count=0;
    TTree* tevents = new TTree("events","V1290 events");
    tevents->Branch("event_id", &e_event_id, "event_id/i");
    tevents->Branch("word_count", &e_word_count, "word_count/i");

    // --- event_stats Tree ---
    UInt_t s_event_id = 0;
    Double_t s_trigger_ns = 0.0;
    Bool_t s_trigger_present = kTRUE;
    Int_t s_n_leading[32] = {0};
    Int_t s_n_trailing[32] = {0};
    TTree* tstat = new TTree("event_stats","Per-event channel counts");
    tstat->Branch("event_id", &s_event_id, "event_id/i");
    tstat->Branch("trigger_ns", &s_trigger_ns, "trigger_ns/D");
    tstat->Branch("trigger_present", &s_trigger_present,"trigger_present/O");
    tstat->Branch("n_leading", s_n_leading, "n_leading[32]/I");
    tstat->Branch("n_trailing", s_n_trailing, "n_trailing[32]/I");

    // ==== Read and Decode File ====
    uint32_t word=0;
    event_v1290 ev{};
    std::memset(&ev, 0, sizeof(ev));
    uint64_t n_words=0, n_events=0, n_hits_written=0;

    while ( ifile.read(reinterpret_cast<char*>(&word), 4) ) {
        ++n_words;
        if (need_bswap) word = __builtin_bswap32(word); // Byte swap if needed
        int ret = Decode(word, &ev);
        if (ret == 1) { // Event is complete
            ++n_events;
            const double trigger_ns = static_cast<double>(ev.trigger_time) * TRIG_LSB_NS;

            // ---- Process hits for this event ----
            for (int ch=0; ch<32; ++ch) {
                const int nHits = ev.hit_count[ch];
                for (int i=0; i<nHits; ++i) {
                    h_identify = TAG_MEAS;
                    h_trailing = ev.tdc_trailing[ch][i];
                    h_ch = ch;
                    h_tdc_raw = ev.tdc_measurement[ch][i];
                    h_event_id = ev.event_id;
                    h_t_ns = static_cast<double>(h_tdc_raw) * MEAS_LSB_NS;
                    h_trigger_ns = trigger_ns;
                    h_dt_ns = h_t_ns - h_trigger_ns;
                    thits->Fill();
                    ++n_hits_written;
                }
            }

            // ---- Fill events tree ----
            e_event_id = ev.event_id;
            e_word_count = ev.word_count;
            tevents->Fill();

            // ---- Fill event_stats tree ----
            std::fill(std::begin(s_n_leading), std::end(s_n_leading), 0);
            std::fill(std::begin(s_n_trailing), std::end(s_n_trailing), 0);

            for (int ch=0; ch<32; ++ch) {
                const int nHits = ev.hit_count[ch];
                for (int i=0; i<nHits; ++i) {
                    if (ev.tdc_trailing[ch][i] == 1) {
                        s_n_trailing[ch] += 1;
                    }
                    if (ev.tdc_trailing[ch][i] == 0) { // Leading edge
                        s_n_leading[ch] += 1;
                    }
                }
            }
            s_event_id = ev.event_id;
            s_trigger_ns = trigger_ns;
            s_trigger_present = (ev.trigger_time != 0) ? kTRUE : kFALSE;
            tstat->Fill();

            // Reset event struct for next event
            std::memset(&ev, 0, sizeof(ev));
        }
    }

    std::cout << "Total words read = " << n_words
              << ", events decoded = " << n_events
              << ", hits written (total individual hits) = " << n_hits_written << "\n";

    // ==== Write ROOT File ====
    fout->cd();
    thits->Write();
    tevents->Write();
    tstat->Write();
    fout->Write();
    fout->Close();
    std::cout << "Data trees saved to " << output_root_name << std::endl;

    // ==== Calculate and Return Efficiency ====
    // Re-open the ROOT file to read the event_stats tree for efficiency calculation
    std::unique_ptr<TFile> fin(TFile::Open(output_root_name.c_str(), "READ"));
    if (!fin || fin->IsZombie()) {
        std::cerr << "Error: Could not re-open ROOT file " << output_root_name << " for efficiency calculation." << std::endl;
        return {};
    }
    TTree* tstat_for_eff = dynamic_cast<TTree*>(fin->Get("event_stats"));
    if (!tstat_for_eff) {
        std::cerr << "Error: Could not retrieve 'event_stats' tree from " << output_root_name << std::endl;
        return {};
    }

    EfficiencyResult eff_result = calculate_efficiency(tstat_for_eff);
    // Print individual file efficiency
    std::cout << "\n--- Efficiency for " << input_filename << " ---" << std::endl;
    std::cout << "Definition: (Events with >=1 hit / Valid Events)" << std::endl;
    std::cout << "Efficiency: " << std::fixed << std::setprecision(6) << eff_result.efficiency << " ± " << eff_result.error << std::endl;
    std::cout << "Events with Hits (Numerator): " << eff_result.numerator << std::endl;
    std::cout << "Total Valid Events (Denominator): " << eff_result.denominator << std::endl;
    std::cout << "--- End File Summary ---\n" << std::endl;

    // File handles will be closed automatically by unique_ptr destructors
    std::cout << "Analysis complete for " << input_filename << ". Output file: " << output_root_name << std::endl;
    return eff_result;
}


int main(int argc, const char* argv[]) {
    gROOT->SetBatch(true); // Essential for running without X11

    bool need_bswap = false; // Default: no swapping
    std::string directory_path = "."; // Default to current directory

    // Parse command-line arguments
    if (argc > 1) {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--bswap" || arg == "-b") {
                need_bswap = true;
                std::cout << "Byte swapping enabled (--bswap flag detected)." << std::endl;
            } else if (arg == "--help" || arg == "-h") {
                 std::cerr << "Usage: " << argv[0] << " [--bswap|-b] [directory_path]" << std::endl;
                 std::cerr << "  Processes all .bin files in the specified directory (default: current directory)." << std::endl;
                 std::cerr << "  Use --bswap or -b if the input data requires 32-bit word byte swapping." << std::endl;
                 return 1;
            } else {
                directory_path = arg; // Assume it's the directory path
                std::cout << "Processing files in directory: " << directory_path << std::endl;
            }
        }
    } else {
         std::cout << "Processing .bin files in the current directory." << std::endl;
    }

    // ==== Find .bin files ====
    std::vector<std::string> bin_files;
    try {
        for (const auto& entry : std::filesystem::directory_iterator(directory_path)) {
            if (entry.is_regular_file() && entry.path().extension() == ".bin") {
                bin_files.push_back(entry.path().string());
            }
        }
    } catch (const std::filesystem::filesystem_error& ex) {
        std::cerr << "Error accessing directory " << directory_path << ": " << ex.what() << std::endl;
        return 1;
    }

    if (bin_files.empty()) {
        std::cerr << "No .bin files found in directory: " << directory_path << std::endl;
        return 1;
    }

    std::cout << "Found " << bin_files.size() << " .bin file(s) to process." << std::endl;

    // ==== Process each .bin file ====
    // Use a vector of tuples to store filename, extracted number, and result for sorting
    std::vector<std::tuple<std::string, double, EfficiencyResult>> results_with_numbers;
    for (const auto& filename : bin_files) {
        EfficiencyResult res = analyze_single_file(filename, need_bswap);
        double voltage = extract_voltage_from_filename(filename);
        results_with_numbers.emplace_back(filename, voltage, res);
    }

    // Sort results by the extracted voltage number
    std::sort(results_with_numbers.begin(), results_with_numbers.end(),
              [](const auto& a, const auto& b) {
                  return std::get<1>(a) < std::get<1>(b); // Sort by the double (voltage)
              });

    // ==== Print Final Summary Table ====
    std::cout << "\n\n========== FINAL EFFICIENCY SUMMARY ==========" << std::endl;
    std::cout << std::setw(50) << std::left << "File Name"
              << std::setw(15) << std::left << "Voltage (kV)"
              << std::setw(12) << std::left << "Efficiency"
              << std::setw(12) << std::left << "Error"
              << std::setw(10) << std::left << "Hits"
              << std::setw(10) << std::left << "Valid Evts" << std::endl;
    std::cout << std::string(110, '-') << std::endl;

    for (const auto& [filename, voltage, result] : results_with_numbers) {
        std::filesystem::path p(filename);
        std::string display_name = p.filename().string();
        std::cout << std::setw(50) << std::left << display_name
                  << std::setw(15) << std::left << std::fixed << std::setprecision(1) << voltage << " kV"
                  << std::setw(12) << std::left << std::fixed << std::setprecision(6) << result.efficiency
                  << std::setw(12) << std::left << std::fixed << std::setprecision(6) << result.error
                  << std::setw(10) << std::left << result.numerator
                  << std::setw(10) << std::left << result.denominator << std::endl;
    }
    std::cout << "==============================================" << std::endl;


    // ==== Create Efficiency Plot ====
    if (results_with_numbers.empty()) {
        std::cerr << "No results to plot." << std::endl;
        return 0;
    }

    // Prepare data for TGraphErrors
    int n_points = results_with_numbers.size();
    std::vector<double> x_values(n_points); // Voltages
    std::vector<double> y_values(n_points); // Efficiencies
    std::vector<double> y_errors(n_points); // Errors on efficiencies
    std::vector<std::string> x_labels(n_points); // Labels like "8 kV"

    for (int i = 0; i < n_points; ++i) {
        const auto& [filename, voltage, result] = results_with_numbers[i];
        x_values[i] = voltage;
        y_values[i] = result.efficiency;
        y_errors[i] = result.error;
        x_labels[i] = std::to_string(static_cast<int>(voltage)) + " kV"; // Format label
    }

    // Create TGraphErrors
    TGraphErrors* graph = new TGraphErrors(n_points, x_values.data(), y_values.data(), nullptr, y_errors.data());
    graph->SetTitle("Efficiency vs Voltage;Voltage (kV);Efficiency");
    graph->SetMarkerStyle(20); // Full circle
    graph->SetMarkerSize(1.2);
    graph->SetMarkerColor(kBlue);
    graph->SetLineColor(kBlue); // Line color for connecting points
    graph->SetLineWidth(2);     // Line width for connecting points

    // Create canvas
    TCanvas* canvas = new TCanvas("canvas", "Efficiency vs Voltage", 1000, 700);
    canvas->SetGridy(); // Add horizontal grid lines

    // Draw graph with lines and points
    graph->Draw("ALP"); // A: Axis, L: Line, P: Points (and error bars)

    // Style the axes
    // X-axis labels are set via the values now, no need for TLatex labels
    graph->GetXaxis()->SetTitleOffset(1.2);
    graph->GetYaxis()->SetRangeUser(0.0, 1.05); // Set Y range from 0 to slightly above 1
    graph->GetYaxis()->SetTitleOffset(1.3);

    // ==== Fit Logistic Curve ====
    // Define a logistic function: f(x) = L / (1 + exp(-k*(x-x0))) + B
    // Where L is the curve's maximum value (we'll fix it to 1 for efficiency)
    // k is the steepness of the curve
    // x0 is the midpoint
    // B is the curve's minimum value (we'll fix it to 0 for efficiency)
    // TF1* fit_func = new TF1("fit_func", "[0] / (1 + exp(-[1]*(x-[2]))) + [3]", x_values.front(), x_values.back());
    // Simpler form fixing L=1 and B=0
    TF1* logistic_fit = new TF1("logistic_fit", "[0] / (1 + exp(-[1]*(x-[2])))", x_values.front() - 1, x_values.back() + 1);
    logistic_fit->SetParameters(1.0, 0.1, x_values[n_points/2]); // Initial guesses: L=1, k=0.1, x0=midpoint voltage
    logistic_fit->SetParNames("L", "k", "x0");
    logistic_fit->SetLineColor(kRed);
    logistic_fit->SetLineWidth(2);

    // Perform the fit
    // Use "Q" for Quiet mode, "R" for Range (fit within function's defined range)
    TFitResultPtr fit_result = graph->Fit("logistic_fit", "SQ"); // S: return TFitResultPtr, Q: Quiet

    if (fit_result.Get()) {
        std::cout << "\n--- Logistic Fit Results ---" << std::endl;
        logistic_fit->Print(); // This prints the parameters and basic info
        // You can access individual parameters like:
        // double L = logistic_fit->GetParameter(0);
        // double k = logistic_fit->GetParameter(1);
        // double x0 = logistic_fit->GetParameter(2);
        // double chi2 = fit_result->Chi2();
        // int ndf = fit_result->Ndf();
        // std::cout << "Chi2/NDF = " << chi2 << "/" << ndf << " = " << (ndf > 0 ? chi2/ndf : 0) << std::endl;
        std::cout << "--- End Fit Results ---\n" << std::endl;
    } else {
        std::cerr << "Warning: Logistic fit failed or was not performed." << std::endl;
    }

    // Update canvas to apply changes (including fit)
    canvas->Update();

    // Save canvas as PDF
    std::string plot_filename = "efficiency_vs_voltage.pdf";
    canvas->SaveAs(plot_filename.c_str());
    std::cout << "\nEfficiency plot saved as: " << plot_filename << std::endl;

    // Cleanup (optional, but good practice)
    delete canvas; // This will also delete primitives drawn on it like the graph and fit function

    return 0;
}