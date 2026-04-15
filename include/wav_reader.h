#pragma once
#include <vector>
#include <string>
#include <cstdint>

struct WavFile {
    std::vector<int16_t> samples; // L, but get this, AND R!
    int total_frames;
};

WavFile load_wav(const std::string& path);