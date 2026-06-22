#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <sys/stat.h>

#include <stdlib.h>
#include <stdio.h>

#include <iostream>
#include <fstream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <unordered_map>
#include <string>
#include <sstream>

#include "TROOT.h"
#include "TFile.h"
#include "TH1.h"
#include "TCanvas.h"
#include "TGraph.h"
#include "TF1.h"

struct event_v1290
{
    uint32_t board_identify;
    uint32_t event_id;
    uint16_t error_flag;
    uint32_t trigger_time;
    int hit_count[32];
    uint8_t tdc_trailing[32][1024];
    uint32_t tdc_measurement[32][1024];
    uint16_t word_count;
};

int Decode(uint32_t data, event_v1290 *ev)
{
    uint8_t tdc_trailing;
    uint8_t identify;
    int tdc_id, tdc_channel;
    uint16_t error_flag = 0, word_count;
    uint32_t tdc_measurement, event_count = -1;
    //uint32_t master_identify, submodule_identify, id;

    identify = data >> 27;
    switch (identify)
    {
    case 0x0: // TDC Measurement
        tdc_trailing = (data & 0x04000000) ? 1 : 0;
        tdc_channel = (data >> 21) & 0x1f;
        tdc_measurement = data & 0x1FFFFF;

        ev->tdc_trailing[tdc_channel][ev->hit_count[tdc_channel]] = tdc_trailing;
        ev->tdc_measurement[tdc_channel][ev->hit_count[tdc_channel]] = tdc_measurement;
        ev->hit_count[tdc_channel]++;
        break;

    case 0x1: // TDC Header
        // tdc_id = (data >> 24) & 0x3;
        // event_id = (data >> 12) & 0xfff;
        // bunch_id = data & 0xfff;
        // std::cout << "TDC Header: TDC: " << tdc_id << " EventID: " << event_id << " bunch_id: " << bunch_id << "\n";
        break;

    case 0x3: // TDC Trailer
              // tdc_id = (data >> 24) & 0x3;
              // ev->event_id = (data >> 12) & 0xfff;
              // word_count = data & 0xfff;
              // std::cout << "TDC Trailer: TDC: " << tdc_id << " EventID: " << event_id << " word_count: " << word_count << "\n";
        break;

    case 0x4: // TDC Error
        tdc_id = (data >> 24) & 0x3;
        error_flag = data & 0x7fff;
        std::cout << "TDC Error: TDC: " << tdc_id << " error_flag: " << std::hex << std::setw(4) << std::setfill('0') << error_flag << std::dec << "\n";
        break;

    case 0x8: // Global header
        event_count = (data >> 5) & 0x3fffff;
        ev->event_id = event_count;
        // std::cout << "Global Header:  event_count: " << event_count << "\n";
        break;

    case 0x10: // Global trailer
        error_flag = (data >> 24) & 0x7;
        word_count = (data >> 5) & 0xffff;
        ev->word_count = word_count;
        ev->trigger_time |= data & 0x1F;
        if (error_flag)
            std::cout << "Global Trailer:  event_count: " << event_count << "  word_count: " << word_count;
        if (error_flag & 0x1)
            std::cout << "  [TDC Error]";
        if (error_flag & 0x2)
            std::cout << "  [OVERFLOW]";
        if (error_flag & 0x4)
            std::cout << "  [TRIGGER LOST]";
        if (error_flag)
            std::cout << "\n";
        // for (ch = 0; ch < 32; ch++)
        //     std::cout << ev->hit_count[ch] << " ";
        // std::cout << "\n";
        return 1;

        break;

    case 0x11: // Global Trigger Time Tag
        // Please check Manual 2.5.1,
        // for old firmware, the resolution of Trigger time is 800ns, it is very recommand update to reversion v0.7 or later
        ev->trigger_time = (data & 0x7ffffff) << 5;
        // std::cout << "trigger_time: " << trigger_time * 25 << " ns\n";
        break;

    case 0x18: // Filler
        // std::cout << "Filler: " << std::hex << std::setw(4) << std::setfill('0') << data << std::dec << "\n";
        break;

    case 0x1F: // User define
        ev->board_identify = data & 0xFFFF0000;
        //master_identify = (data >> 21) & 0x3F;
        //submodule_identify = (data >> 16) & 0x1F;
        //id = data & 0xFFFF;
        // std::cout << "board_identify: " << std::hex << std::setw(8) << std::setfill('0')  << ev->board_identify << std::dec  << "\n";
        // std::cout << "master_identify: " << master_identify << "  submodule_identify: " << submodule_identify << "  id: " << id << "\n";
        break;

    default:
        std::cout << "Unknown data: " << std::hex << std::setw(4) << std::setfill('0') << data << std::dec << "\n";
        break;
    }
    return 0;
}

int main(int argc, char *argv[])
{
    /* Open rawdata file */
    std::string filename = "rawdata.bin";
    std::fstream ifile;
    if(argc > 1)
        filename = argv[1];
    ifile.open(filename, std::fstream::in | std::fstream::binary);
    if (!ifile.is_open())
    {
        std::cout << "Opening file " << filename << " failed" << std::endl;
        return -1;
    }

    /* Prepare ROOT Canvas & Histogram */
    int ch, hit;
    int w = 3000, h = 3000;
    std::string pdf_path = "out.pdf";
    TCanvas *c1 = new TCanvas("c1", "event", w, h);
    c1->SetCanvasSize(w, h);
    c1->Print((pdf_path + "[").c_str());
    double xstart = 0, xend = 2000;
    TH1D *th1[32];
    std::string name, title;

    /* Naming Histogram with channel number */
    for (ch = 0; ch < 32; ch++)
    {
        name = "measure_" + std::to_string(ch);
        title = "TDC CH" + std::to_string(ch) + ";Time [ns];Count";
        th1[ch] = new TH1D(name.c_str(), title.c_str(), 100, xstart, xend);
    }

    /* Loop read all data, fill to Histogram */
    /* This loop is a bad design */
    int ret;
    uint32_t data = 0;
    struct event_v1290 event;
    memset(&event, 0, sizeof(event));
    while (1)
    {
        if (!ifile)
            break;
        ifile.read((char *)&data, 4);
        ret = Decode(data, &event); 
        if (ret == 1)
        {
            /* Decode return 1 if a completed event is stored in struct event_v1290 */
            for (ch = 0; ch < 32; ch++)
                for (hit = 0; hit < event.hit_count[ch]; hit++)
                    th1[ch]->Fill(event.tdc_measurement[ch][hit] * 25. / 1024.);
            memset(&event, 0, sizeof(struct event_v1290));
        }
    }

    /* Draw hisogram */
    for (ch = 0; ch < 32; ch++)
    {
        th1[ch]->Draw();
        c1->Print(pdf_path.c_str());
    }
    c1->Print((pdf_path + "]").c_str());

    ifile.close();
    return 0;
}
