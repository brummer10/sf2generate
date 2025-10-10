
/*
 * SoundFontGen.h
 *
 * SPDX-License-Identifier:  BSD-3-Clause
 *
 * Copyright (C) 2025 brummer <brummer@web.de>
 */

/****************************************************************
  SoundFontGen - a minimal C++ Sound Font generator

  generate a minimal sf2 binary from a wav file
****************************************************************/

#include <fstream>
#include <vector>
#include <string>
#include <iostream>
#include <cstdint>
#include <cstring>
#include <cmath>

#include <cassert>

#include <sndfile.hh>

#pragma once

#ifndef SOUNDFONTGEN_H
#define SOUNDFONTGEN_H

class AudioConvert {
public:
    std::vector<int16_t> data;
    std::vector<int16_t> loop_data;
    uint32_t channels;
    uint32_t samplesize;
    uint32_t sampleRate;
    
    AudioConvert() {
        channels   = 0;
        samplesize = 0;
        sampleRate = 0;
        data.clear();
    }
    
    ~AudioConvert() {}

    // load a Audio File into the buffer
    inline bool load(const std::string& file) {
        SF_INFO info;
        info.format = 0;

        channels = 0;
        samplesize = 0;
        sampleRate = 0;
        float *samples = nullptr;
        // Open the wave file for reading
        SNDFILE *sndfile = sf_open(file.c_str(), SFM_READ, &info);

        if (!sndfile) {
            std::cerr << "Error: could not open file " << sf_error (sndfile) << std::endl;
            return false;
        }
        if (info.channels > 1) {
            std::cerr << "Warning: only the first channel is used!" << std::endl;
        }
        try {
            samples = new float[info.frames * info.channels];
        } catch (...) {
            std::cerr << "Error: could not load file" << std::endl;
            return false;
        }
        samplesize = (uint32_t) sf_readf_float(sndfile, &samples[0], info.frames);
        if (!samplesize ) samplesize = info.frames;
        channels = info.channels;
        sampleRate = info.samplerate;
        sf_close(sndfile);
        for (uint32_t i = 0; i<samplesize; i++) {
            data.push_back(floatToInt16(samples[i * info.channels]));
        }
        delete[] samples;
        if (!data.empty()) {
            loop_data.assign(data.begin(), data.end());
            //croosfade();
        }
        return !data.empty();
    }
    
    inline bool convert(const float *samples, const uint32_t samplerate,
            const uint32_t samplesize, const uint32_t loop_l, const uint32_t loop_r) {
        data.clear();
        sampleRate = samplerate;
        for (uint32_t i = 0; i<samplesize; i++) {
            data.push_back(floatToInt16(samples[i]));
        }
        if (!data.empty()) {
            loop_data.assign(data.begin() + loop_l, data.begin() + loop_r);
            //croosfade();
        }
        return !data.empty();
    }

private:
    // convert float to short (16 bit) 
    inline int16_t floatToInt16(float x) {
        x = std::fmax(-1.0f, std::fmin(1.0f, x));
        return static_cast<int16_t>(std::lrintf(x * 32767.0f));
    }

    inline void croosfade() {
        size_t fadeLen = std::min<size_t>(256, loop_data.size() / 10);
        if (fadeLen == 0) return;
        // fade in at loop start
        for (size_t i = 0; i < fadeLen; ++i) {
            float gain = static_cast<float>(i) / static_cast<float>(fadeLen);
            loop_data[i] *= gain;
        }
        // fade out at loop end - fade length 
        for (size_t i = loop_data.size() - fadeLen; i < loop_data.size(); ++i) {
            float gain = 1.0f - static_cast<float>(i) / static_cast<float>(fadeLen);
            loop_data[i] *= gain;
        }
    }
};

class SoundFontWriter {
public:

    // takes a audio file and convert it to mono 16 bit when needed and write it into a SoundFont (sf2)
    bool write_sf2(const std::string& filename, const std::string& sf2file,
                            const std::string& name, uint8_t rootNote = 60,
                            const uint16_t Chorus = 500, const uint16_t Reverb = 500,
                            const int16_t pitchCorrection = 0) {
        if (!sample.load(filename)) {
            std::cerr << "Failed to read wav file or unsupported format!\n";
            return false;
        }
        loop_left = 0;
        loop_right = sample.data.size();
        rootKey = rootNote;
        chorus = Chorus;
        reverb = Reverb;
        chPitchCorrection = pitchCorrection;
        return write_sf2(sf2file, name);
    }

    // takes a audio float buffer as OneShoot instrument
    // clip the buffer for looping to the given size (loop_l - loop_r)
    // convert it to int16_t
    // optional set the root Key to use, default is 60 (C4)
    // write the SoundFont (sf2)
    bool generate_sf2(const float *samples, const uint32_t loop_l, const uint32_t loop_r,
                    const uint32_t samplesize, const uint32_t samplerate,
                    const std::string& sf2file, const std::string& name,
                    const uint8_t rootNote = 60, const uint16_t Chorus = 500,
                    const uint16_t Reverb = 500, const int16_t pitchCorrection = 0) {

        if (!sample.convert(samples, samplerate, samplesize, loop_l, loop_r)) {
            std::cerr << "Failed to read audio buffer or unsupported format!\n";
            return false;
        }
        loop_left = loop_l;
        loop_right = loop_r;
        rootKey = rootNote;
        chorus = Chorus;
        reverb = Reverb;
        chPitchCorrection = pitchCorrection;
        return write_sf2(sf2file, name);
    }


    SoundFontWriter() {
        loop_left = 0;
        loop_right = 0;
        rootKey = 60;
        chorus = 500;
        reverb = 500;
        chPitchCorrection = 0;
    };
    ~SoundFontWriter(){};

private:
    AudioConvert sample;

    std::vector<uint8_t> info;
    std::vector<uint8_t> sdta;
    std::vector<uint8_t> pdta;
    std::vector<uint8_t> riff;
    std::vector<std::vector<uint8_t>> pdta_chunks;

    uint32_t loop_left;
    uint32_t loop_right;
    uint8_t  rootKey;
    uint16_t chorus;
    uint16_t reverb;
    int16_t chPitchCorrection;

    // Buffer helpers for little-endian binary writing
    template<typename T>
    void write(std::vector<uint8_t>& buf, T v) {
        for (size_t i=0; i<sizeof(T); ++i) buf.push_back(uint8_t((reinterpret_cast<uint8_t*>(&v))[i]));
    }

    void write_str(std::vector<uint8_t>& buf, const char* s, size_t n) {
        for (size_t i=0; i<n; ++i) buf.push_back(s[i]);
    }

    void write_strz(std::vector<uint8_t>& buf, const std::string& s, size_t n) {
        for (size_t i=0; i<n; ++i) buf.push_back(i<s.size()?s[i]:0);
    }

    void write_info(const std::string& name) {
        info.clear();
        write_str(info, "LIST", 4); write<uint32_t>(info, 0); // placeholder
        write_str(info, "INFO", 4);
        write_str(info, "ifil", 4); write<uint32_t>(info, 4); write<uint16_t>(info, 2); write<uint16_t>(info, 1);
        write_str(info, "isng", 4); write<uint32_t>(info, 10); write_strz(info, "EMU8000", 10);
        write_str(info, "INAM", 4); write<uint32_t>(info, 20); write_strz(info, name, 20);
        write_str(info, "ICRD", 4); write<uint32_t>(info, 10); write_strz(info, "2025", 10);
        uint32_t info_size = static_cast<uint32_t>(info.size()) - 8;
        std::memcpy(&info[4], &info_size, 4);
    }

    void write_sdta() {
        sdta.clear();
        write_str(sdta, "LIST", 4); write<uint32_t>(sdta, 0); // placeholder
        write_str(sdta, "sdta", 4);
        write_str(sdta, "smpl", 4); write<uint32_t>(sdta, 0); // placeholder
        size_t smpl_offset = sdta.size();

        for (int i=0; i<16; ++i) write<int16_t>(sdta, 0);
        sdta.insert(sdta.end(), reinterpret_cast<const uint8_t*>(sample.data.data()),
            reinterpret_cast<const uint8_t*>(sample.data.data()) + sample.data.size()*2);

        for (int i=0; i<16; ++i) write<int16_t>(sdta, 0);

        sdta.insert(sdta.end(), reinterpret_cast<const uint8_t*>(sample.loop_data.data()),
            reinterpret_cast<const uint8_t*>(sample.loop_data.data()) + sample.loop_data.size()*2);

        for (int i=0; i<16; ++i) write<int16_t>(sdta, 0);

        uint32_t smpl_len = static_cast<uint32_t>(sdta.size() - smpl_offset);
        std::memcpy(&sdta[sdta.size() - smpl_len - 4], &smpl_len, 4);
        uint32_t sdta_size = static_cast<uint32_t>(sdta.size()) - 8;
        std::memcpy(&sdta[4], &sdta_size, 4);
    }

    void write_phdr() {
        // phdr (38*3)
        std::vector<uint8_t> phdr;
        write_str(phdr, "phdr", 4); write<uint32_t>(phdr, 38*3);
        // Preset 0: OneShot
        write_strz(phdr, "OneShot", 20);   // preset name
        write<uint16_t>(phdr, 0);          // wPreset
        write<uint16_t>(phdr, 0);          // wBank
        write<uint16_t>(phdr, 0);          // wPresetBagNdx
        write<uint32_t>(phdr, 0);          // dwLibrary
        write<uint32_t>(phdr, 0);          // dwGenre
        write<uint32_t>(phdr, 0);          // dwMorphology
        // Preset 1: Looped
        write_strz(phdr, "Looped", 20);    // preset name
        write<uint16_t>(phdr, 1);          // wPreset
        write<uint16_t>(phdr, 0);          // wBank
        write<uint16_t>(phdr, 1);          // wPresetBagNdx
        write<uint32_t>(phdr, 0);          // dwLibrary
        write<uint32_t>(phdr, 0);          // dwGenre
        write<uint32_t>(phdr, 0);          // dwMorphology
        // Terminator EOP
        write_strz(phdr, "EOP", 20);       // preset name
        write<uint16_t>(phdr, 0);          // wPreset
        write<uint16_t>(phdr, 0);          // wBank
        write<uint16_t>(phdr, 2);          // wPresetBagNdx (should point past end)
        write<uint32_t>(phdr, 0);          // dwLibrary
        write<uint32_t>(phdr, 0);          // dwGenre
        write<uint32_t>(phdr, 0);          // dwMorphology
        //assert(phdr.size() == 8 + 38*3);
        pdta_chunks.push_back(std::move(phdr));
    }

    void write_pbag() {
        // pbag (4*3)
        std::vector<uint8_t> pbag;
        write_str(pbag, "pbag", 4); write<uint32_t>(pbag, 4*3);
        // preset 0 bag (points to pgen index 0)
        write<uint16_t>(pbag, 0); write<uint16_t>(pbag, 0);
        // preset 1 bag (points to pgen index 1)
        write<uint16_t>(pbag, 1); write<uint16_t>(pbag, 0);
        // preset 2 bag (points to pgen index 2)
        write<uint16_t>(pbag, 2); write<uint16_t>(pbag, 0);
        //assert(pbag.size() == 8 + 12);
        pdta_chunks.push_back(std::move(pbag));
    }

    void write_pmod() {
        // pmod (10 bytes)
        std::vector<uint8_t> pmod;
        write_str(pmod, "pmod", 4); write<uint32_t>(pmod, 10);
        for (int i=0; i<10; ++i) write<uint8_t>(pmod, 0);
        //assert(pmod.size() == 8 + 10);
        pdta_chunks.push_back(std::move(pmod));
    }

    void write_pgen() {
        // pgen (4*3)
        std::vector<uint8_t> pgen;
        write_str(pgen, "pgen", 4); write<uint32_t>(pgen, 4*3);
        // preset 0 -> instrument 0
        write<uint16_t>(pgen, 41); write<uint16_t>(pgen, 0);
        // preset 1 -> instrument 1
        write<uint16_t>(pgen, 41); write<uint16_t>(pgen, 1);
        // terminator
        write<uint16_t>(pgen, 0); write<uint16_t>(pgen, 0);
        //assert(pgen.size() == 8 + 12);
        pdta_chunks.push_back(std::move(pgen));
    }

    void write_inst() {
        // inst (22*3)
        std::vector<uint8_t> inst;
        write_str(inst, "inst", 4); write<uint32_t>(inst, 22*3);
        write_strz(inst, "OneShot", 20); write<uint16_t>(inst, 0);
        write_strz(inst, "Looped", 20); write<uint16_t>(inst, 1);
        write_strz(inst, "EOI", 20); write<uint16_t>(inst, 2);
        //assert(inst.size() == 8 + 66);
        pdta_chunks.push_back(std::move(inst));
    }

    void write_ibag() {
        // ibag (4*3)
        std::vector<uint8_t> ibag;
        write_str(ibag, "ibag", 4); write<uint32_t>(ibag, 4*3);
        // instrument 0 (OneShot) uses igen records starting at index 0
        write<uint16_t>(ibag, 0); write<uint16_t>(ibag, 0);
        // instrument 1 (Looped) uses igen records starting at index 2
        write<uint16_t>(ibag, 4); write<uint16_t>(ibag, 0);
        // terminator: points to igen index 4 (end)
        write<uint16_t>(ibag, 8); write<uint16_t>(ibag, 0);
        //assert(ibag.size() == 8 + 12);
        pdta_chunks.push_back(std::move(ibag));
    }

    void write_imod() {
        // imod (10 bytes)
        std::vector<uint8_t> imod;
        write_str(imod, "imod", 4); write<uint32_t>(imod, 10);
        for (int i=0; i<10; ++i) write<uint8_t>(imod, 0);
        //assert(imod.size() == 8 + 10);
        pdta_chunks.push_back(std::move(imod));
    }

    void write_igen() {
        // igen (4*5)
        std::vector<uint8_t> igen;
        write_str(igen, "igen", 4); write<uint32_t>(igen, 4*9);
        // Instrument 0 (OneShot)
        write<uint16_t>(igen, 15); write<uint16_t>(igen, chorus);  // Chorus send 50%
        write<uint16_t>(igen, 16); write<uint16_t>(igen, reverb);  // Reverb send 50%
        write<uint16_t>(igen, 54); write<uint16_t>(igen, 0);    // SampleModes = OneShoot
        write<uint16_t>(igen, 53); write<uint16_t>(igen, 0);    // SampleID, 0
        // Instrument 1 (Looped)
        write<uint16_t>(igen, 15); write<uint16_t>(igen, chorus);  // Chorus send 50%
        write<uint16_t>(igen, 16); write<uint16_t>(igen, reverb);  // Reverb send 50%
        write<uint16_t>(igen, 54); write<uint16_t>(igen, 1);    // SampleModes = Standard Loop
        write<uint16_t>(igen, 53); write<uint16_t>(igen, 1);    // SampleID, 1
        // global terminator
        write<uint16_t>(igen, 0); write<uint16_t>(igen, 0);
        //assert(igen.size() == 8 + 44);
        pdta_chunks.push_back(std::move(igen));
    }

    void write_shdr(const std::string& name) {
        // shdr (46*2)
        std::vector<uint8_t> shdr;
        write_str(shdr, "shdr", 4); write<uint32_t>(shdr, 46*3);
        // Real sample header (46 bytes)
        write_strz(shdr, "OneShoot", 20);                             // 20
        write<uint32_t>(shdr, 16);                                    // dwStart
        write<uint32_t>(shdr, 16 + (uint32_t)sample.data.size()-1);   // dwEnd
        write<uint32_t>(shdr, 16);                                    // dwStartLoop
        write<uint32_t>(shdr, 16 + (uint32_t)sample.data.size()-1);   // dwEndLoop
        write<uint32_t>(shdr, sample.sampleRate);                     // dwSampleRate
        write<uint8_t>(shdr, rootKey);                                // byOriginalPitch
        write<int8_t>(shdr, chPitchCorrection);                       // chPitchCorrection
        write<uint16_t>(shdr, 0);                                     // wSampleLink
        write<uint16_t>(shdr, 1);                                     // sfSampleType (mono)
        // Real sample header (46 bytes)
        write_strz(shdr, "Loop", 20);                                   // 20
        write<uint32_t>(shdr, 32 +  (uint32_t)sample.data.size());     // dwStart
        write<uint32_t>(shdr, 32 + (uint32_t)sample.data.size() + sample.loop_data.size()-1);     // dwEnd
        write<uint32_t>(shdr, 32 + (uint32_t)sample.data.size());     // dwStartLoop
        write<uint32_t>(shdr, 32 + (uint32_t)sample.data.size() + sample.loop_data.size()-1);    // dwEndLoop
        write<uint32_t>(shdr, sample.sampleRate);                     // dwSampleRate
        write<uint8_t>(shdr, rootKey);                                // byOriginalPitch
        write<int8_t>(shdr, chPitchCorrection);                       // chPitchCorrection
        write<uint16_t>(shdr, 0);                                     // wSampleLink
        write<uint16_t>(shdr, 1);                                     // sfSampleType (mono)
        // Terminal sample header (46 bytes)
        write_strz(shdr, "EOS", 20);
        for (int i=0; i<4; ++i) write<uint32_t>(shdr, 0);             // start, end, startLoop, endLoop
        write<uint32_t>(shdr, 0);                                     // dwSampleRate
        write<uint8_t>(shdr, 0);                                      // byOriginalPitch
        write<int8_t>(shdr, 0);                                       // chPitchCorrection
        write<uint16_t>(shdr, 0);                                     // wSampleLink
        write<uint16_t>(shdr, 1);                                     // sfSampleType
        //assert(shdr.size() == 8 + 138);
        pdta_chunks.push_back(std::move(shdr));
    }

    void write_pdta() {
        // pdta LIST chunk (buffer and patch size after)
        pdta.clear();
        write_str(pdta, "LIST", 4); write<uint32_t>(pdta, 0); // placeholder
        write_str(pdta, "pdta", 4);
        for (const auto& chunk : pdta_chunks)
            pdta.insert(pdta.end(), chunk.begin(), chunk.end());
        uint32_t pdta_size = static_cast<uint32_t>(pdta.size()) - 8;
        std::memcpy(&pdta[4], &pdta_size, 4);
    }

    void write_riff() {
        // --- Build the full RIFF file in memory
        riff.clear();
        write_str(riff, "RIFF", 4); write<uint32_t>(riff, 0); // placeholder
        write_str(riff, "sfbk", 4);
        riff.insert(riff.end(), info.begin(), info.end());
        riff.insert(riff.end(), sdta.begin(), sdta.end());
        riff.insert(riff.end(), pdta.begin(), pdta.end());
        uint32_t riff_size = static_cast<uint32_t>(riff.size()) - 8;
        std::memcpy(&riff[4], &riff_size, 4);
    }

    bool write_to_disk(const std::string& sf2file) {
        // Write to disk
        std::ofstream outf(sf2file, std::ios::binary);
        if (!outf) return false;
        outf.write(reinterpret_cast<const char*>(riff.data()), riff.size());
        outf.close();
        return true;
    }

    bool write_sf2(const std::string& sf2file, const std::string& name) {
        pdta_chunks.clear();
        write_info(name);
        write_sdta();
        write_phdr();
        write_pbag();
        write_pmod();
        write_pgen();
        write_inst();
        write_ibag();
        write_imod();
        write_igen();
        write_shdr(name);
        write_pdta();
        write_riff();
        return write_to_disk(sf2file);
    }
};

#endif
