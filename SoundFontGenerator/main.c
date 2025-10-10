/*
 * main.c
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 *
 * Copyright (C) 2025 brummer <brummer@web.de>
 */


#include <cmath>
#include <vector>
#include <signal.h>
#include <cstdio>
#include <algorithm>
#include <unistd.h>
#include <iostream>
#include <string>
#include <condition_variable>

#include "ParallelThread.h"
#include "SoundEdit.h"
#include "xpa.h"

SoundEditUi ui;

// the portaudio server process callback
static int process(const void* inputBuffer, void* outputBuffer,
    unsigned long frames, const PaStreamCallbackTimeInfo* timeInfo,
    PaStreamCallbackFlags statusFlags, void* data) {

    float* out = static_cast<float*>(outputBuffer);
    static std::condition_variable *Sync = static_cast<std::condition_variable*>(data);
    static float fRec0[2] = {0};
    static float ramp = 0.0;
    static const float ramp_step = 256.0;
    (void) timeInfo;
    (void) statusFlags;

    if (( ui.af.samplesize && ui.af.samples != nullptr) && ui.play && ui.ready) {
        float fSlow0 = 0.0010000000000000009 * ui.gain;
        for (uint32_t i = 0; i<(uint32_t)frames; i++) {
            fRec0[0] = fSlow0 + 0.999 * fRec0[1];
            for (uint32_t c = 0; c < ui.af.channels; c++) {
                if (!c) {
                    *out++ = ui.af.samples[ui.position*ui.af.channels] * fRec0[0];
                    if (ui.af.channels ==1) *out++ = ui.af.samples[ui.position*ui.af.channels] * fRec0[0];
                } else *out++ = ui.af.samples[ui.position*ui.af.channels+c] * fRec0[0];
            }
            fRec0[1] = fRec0[0];
            // track play-head position
            ui.position++;
            if (ui.position > ui.loopPoint_r) {
                ui.position = ui.loopPoint_l;
                //ui.loadFile();
            } else if (ui.position <= ui.loopPoint_l) {
                ui.position = ui.loopPoint_r;
               // ui.loadFile();
            // ramp up on loop start point
            } else if (ui.position < ui.loopPoint_l + ramp_step) {
                if (ramp < ramp_step) {
                    ++ramp;
                }
                const float fade = max(0.0,ramp) /ramp_step ;
                *(out - 2) *= fade;
                *(out - 1) *= fade;
            // ramp down on loop end point
            } else if (ui.position > ui.loopPoint_r - ramp_step) {
                if (ramp > 0.0) {
                    --ramp; 
                }
                const float fade = max(0.0,ramp) /ramp_step ;
                *(out - 2) *= fade;
                *(out - 1) *= fade;
            }
        }
    } else {
        memset(out, 0.0, (uint32_t)frames * 2 * sizeof(float));
    }
    Sync->notify_one();

    return 0;
}

#if defined(__linux__) || defined(__FreeBSD__) || \
    defined(__NetBSD__) || defined(__OpenBSD__)
// catch signals and exit clean
void
signal_handler (int sig)
{
    switch (sig) {
        case SIGINT:
        case SIGHUP:
        case SIGTERM:
        case SIGQUIT:
            std::cerr << "\nsignal "<< sig <<" received, exiting ...\n"  <<std::endl;
            XLockDisplay(ui.w->app->dpy);
            ui.onExit();
            XFlush(ui.w->app->dpy);
            XUnlockDisplay(ui.w->app->dpy);
        break;
        default:
        break;
    }
}
#endif

int main(int argc, char *argv[]){
    if (argc > 1) {
        std::string cmd = argv[1];
        if ((cmd.compare("--help") == 0) || (cmd.compare("-h") == 0)) {
            std::cout << "Minimal SF2 (SoundFont 2) writer for a single mono WAV file." << std::endl;
            std::cout << "  Usage: " << argv[0] << " input.wav output.sf2\n";
            std::cout << "  optional additional arguments been:\n";
            std::cout << "  RootKey Chorus Reverb\n";
            std::cout << "  RootKey Chorus Reverb\n";
            std::cout << "  given as value in that order\n";
            std::cout << "  RootKey in the Range from 0 to 127\n";
            std::cout << "  Chorus and reverb in the range from 0 to 100\n";
            return 0;
        }
    }

    if (argc > 2) {
        switch (argc) {
            case (3):
            {
                if (!ui.af.swf.write_sf2(argv[1], argv[2], "Sample")) {
                    std::cerr << "Failed to write sf2!\n";
                    return 1;
                }
                std::cout << "SF2 created: " << argv[2] << "\n";
                break;
            }
            case (4):
            {
                if (!ui.af.swf.write_sf2(argv[1], argv[2], "Sample",
                                            (uint8_t)atoi(argv[3]))) {
                    std::cerr << "Failed to write sf2!\n";
                    return 1;
                }
                std::cout << "SF2 created: " << argv[2] << "\n";
                break;
            }
            case (5):
            {
                if (!ui.af.swf.write_sf2(argv[1], argv[2], "Sample", 
                        (uint8_t)atoi(argv[3]), (uint16_t)atoi(argv[4])*10)) {
                    std::cerr << "Failed to write sf2!\n";
                    return 1;
                }
                std::cout << "SF2 created: " << argv[2] << "\n";
                break;
            }
            case (6):
            {
                if (!ui.af.swf.write_sf2(argv[1], argv[2], "Sample",
                        (uint8_t)atoi(argv[3]), (uint16_t)atoi(argv[4])*10,
                                                (uint16_t)atoi(argv[5])*10)) {
                    std::cerr << "Failed to write sf2!\n";
                    return 1;
                }
                std::cout << "SF2 created: " << argv[2] << "\n";
                break;
            }
            default:
            break;
        }
        return 0;
    }

    #if defined(__linux__) || defined(__FreeBSD__) || \
        defined(__NetBSD__) || defined(__OpenBSD__)
    if(0 == XInitThreads()) 
        std::cerr << "Warning: XInitThreads() failed\n" << std::endl;
    #endif

    Xputty app;
    std::condition_variable Sync;

    main_init(&app);
    ui.createGUI(&app);

    #if defined(__linux__) || defined(__FreeBSD__) || \
        defined(__NetBSD__) || defined(__OpenBSD__)
    signal (SIGQUIT, signal_handler);
    signal (SIGTERM, signal_handler);
    signal (SIGHUP, signal_handler);
    signal (SIGINT, signal_handler);
    #endif

    XPa xpa ("sf2generate");
    if(!xpa.openStream(0, 2, &process, (void*) &Sync)) ui.onExit();

    ui.setJackSampleRate(xpa.getSampleRate());

    if(!xpa.startStream()) ui.onExit();
    ui.setPaStream(xpa.getStream());

    if (argv[1])
    #ifdef __XDG_MIME_H__
    if(strstr(xdg_mime_get_mime_type_from_file_name(argv[1]), "audio")) {
    #else
    if( access(argv[1], F_OK ) != -1 ) {
    #endif
        ui.dialog_response(ui.w, (void*) &argv[1]);
    }

    main_run(&app);

    ui.pa.stop();
    main_quit(&app);
    xpa.stopStream();
    printf("bye bye\n");
    return 0;
}

