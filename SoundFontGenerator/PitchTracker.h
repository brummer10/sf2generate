/*
 * PitchTracker.h
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 *
 * Copyright (C) 2025 brummer <brummer@web.de>
 */

#include <fftw3.h>
#include <cmath>
#include <algorithm>
#include <vector>
#include <cstdint>

#pragma once

#ifndef PITCHTRACKER_H
#define PITCHTRACKER_H

class PitchTracker {
public:
    // Estimate dominant pitch of a audio buffer and return the resulting MIDI key.
    uint8_t getPitch( const float* buffer, size_t N,
            float sampleRate, int16_t* pitchCorrection = nullptr,
            float* frequency = nullptr, float minFreq = 20.0f,
            float maxFreq = 5000.0f) {

        if (N < 2) {
            if (pitchCorrection) *pitchCorrection = 0;
            if (frequency) *frequency = 0.0f;
            return 0;
        }

        // --- Allocate FFTW buffers
        fftwf_complex* out = (fftwf_complex*) fftwf_malloc(sizeof(fftwf_complex) * (N/2 + 1));
        float* in = (float*) fftwf_malloc(sizeof(float) * N);
        fftwf_plan plan = fftwf_plan_dft_r2c_1d(N, in, out, FFTW_ESTIMATE);

        // Remove DC offset & apply Hann window
        float mean = 0.0f;
        for (size_t i = 0; i < N; ++i) mean += buffer[i];
        mean /= N;

        for (size_t i = 0; i < N; ++i) {
            float w = 0.5f - 0.5f * std::cos(2.0f * M_PI * i / (N - 1));
            in[i] = (buffer[i] - mean) * w;
        }

        // Execute FFT
        fftwf_execute(plan);

        // Limit frequency search range
        size_t minBin = std::max<size_t>(1, static_cast<size_t>(std::floor(minFreq * N / sampleRate)));
        size_t maxBin = std::min<size_t>(N/2, static_cast<size_t>(std::ceil(maxFreq * N / sampleRate)));

        // Find peak magnitude in spectrum
        size_t peakIndex = 0;
        float peakMag = 0.0f;
        std::vector<float> mags(N/2 + 1);

        for (size_t k = minBin; k <= maxBin; ++k) {
            float re = out[k][0];
            float im = out[k][1];
            float mag = re * re + im * im;
            mags[k] = mag;
            if (mag > peakMag) {
                peakMag = mag;
                peakIndex = k;
            }
        }

        // No peak found
        if (peakIndex == 0) {
            fftwf_destroy_plan(plan);
            fftwf_free(in);
            fftwf_free(out);
            if (pitchCorrection) *pitchCorrection = 0;
            if (frequency) *frequency = 0.0f;
            return 0;
        }

        // Parabolic interpolation
        float interpolatedIndex = static_cast<float>(peakIndex);
        if (peakIndex > 0 && peakIndex < N/2) {
            float alpha = std::log(mags[peakIndex - 1] + 1e-12f);
            float beta  = std::log(mags[peakIndex]     + 1e-12f);
            float gamma = std::log(mags[peakIndex + 1] + 1e-12f);
            float p = 0.5f * (alpha - gamma) / (alpha - 2*beta + gamma);
            interpolatedIndex += p;
        }

        // Convert peak bin to frequency
        float freq = interpolatedIndex * sampleRate / N;
        if (frequency) *frequency = freq;

        if (freq <= 0.0f) {
            fftwf_destroy_plan(plan);
            fftwf_free(in);
            fftwf_free(out);
            if (pitchCorrection) *pitchCorrection = 0;
            return 0;
        }

        // Convert frequency to MIDI note 
        float midiFloat = 69.0f + 12.0f * std::log2(freq / 440.0f);
        int midiNote = static_cast<int>(std::floor(midiFloat + 0.5f));
        midiNote = std::clamp(midiNote, 0, 127);

        // Calculate pitch correction in cents
        double targetFreq = 440.0 * std::pow(2.0, (midiNote - 69) / 12.0);
        double cents = 1200.0 * std::log2(freq / targetFreq);

        // If correction is outside ±50 cents, shift note and recalc
        if (cents > 50.0) {
            if (midiNote < 127) midiNote++;
            targetFreq = 440.0 * std::pow(2.0, (midiNote - 69) / 12.0);
            cents = 1200.0 * std::log2(freq / targetFreq);
        } else if (cents < -50.0) {
            if (midiNote > 0) midiNote--;
            targetFreq = 440.0 * std::pow(2.0, (midiNote - 69) / 12.0);
            cents = 1200.0 * std::log2(freq / targetFreq);
        }

        // Round and clamp correction to valid SF2 range
        int16_t correction = static_cast<int16_t>(std::lround(cents));
        correction = std::clamp<int16_t>(correction, -50, 50);
        if (pitchCorrection) *pitchCorrection = correction;

        // Cleanup
        fftwf_destroy_plan(plan);
        fftwf_free(in);
        fftwf_free(out);

        return static_cast<uint8_t>(midiNote);
    }

private:

};

#endif
